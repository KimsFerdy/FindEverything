#include "app/IndexService.h"

#include <malloc.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <unordered_set>

#include "core/IndexFile.h"

namespace fe {

namespace {

using namespace std::chrono_literals;

constexpr auto kQuietTime = 300ms;  // publish once things calm down...
constexpr auto kMaxDelay = 2s;      // ...but at least this often during a storm
constexpr auto kSaveEvery = 60s;
constexpr auto kMoveTimeout = 500ms;  // MOVED_FROM without a MOVED_TO = moved somewhere we don't watch

uint8_t flagsFor(const struct stat& st) {
    uint8_t flags = 0;
    if (S_ISDIR(st.st_mode)) flags |= kIsDir;
    if (S_ISLNK(st.st_mode)) flags |= kIsSymlink;
    if (S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) flags |= kIsExecutable;
    return flags;
}

}  // namespace

IndexService::IndexService(IndexServiceConfig config, std::function<void()> wake)
    : config_(std::move(config)), wake_(std::move(wake)), wakeFd_(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
    builder_ = std::thread([this] { buildLoop(); });
    worker_ = std::thread([this] { run(); });
}

IndexService::~IndexService() {
    stop_ = true;
    progress_.cancel = true;  // stops a running scan between directories
    poke();
    if (worker_.joinable()) worker_.join();
    {
        std::lock_guard lock(jobMutex_);
        stopBuilder_ = true;
    }
    jobCv_.notify_one();
    if (builder_.joinable()) builder_.join();
    if (wakeFd_ >= 0) close(wakeFd_);
}

void IndexService::poke() {
    const uint64_t one = 1;
    if (wakeFd_ >= 0) {
        [[maybe_unused]] ssize_t n = write(wakeFd_, &one, sizeof one);
    }
}

void IndexService::rescan() {
    rescanRequested_ = true;
    poke();
}

std::unique_ptr<IndexSnapshot> IndexService::takeSnapshot() {
    std::lock_guard lock(snapshotMutex_);
    return std::move(snapshot_);
}

// ---------------------------------------------------------------------------------------------- worker

void IndexService::run() {
    if (!config_.cachePath.empty()) {
        auto cached = std::make_unique<Index>();
        IndexFileInfo info;
        if (loadIndex(*cached, config_.cachePath, &info) && info.source == config_.source) {
            index_ = std::move(cached);
            publish(IndexSnapshot::Origin::Cache, info.createdAt);
        }
    }

    if (!index_ || config_.scanOnStart) {
        if (!scan()) return;
    } else {
        scanning_ = false;
    }
    if (config_.live) setupWatches();
    lastSave_ = Clock::now();

    while (!stop_) {
        pollfd fds[2] = {{wakeFd_, POLLIN, 0}, {inotify_ ? inotify_->fd() : -1, POLLIN, 0}};
        poll(fds, 2, dirty_ || !pendingMoves_.empty() ? 100 : 1000);
        uint64_t drained;
        while (read(wakeFd_, &drained, sizeof drained) > 0) {
        }
        if (stop_) break;

        if (rescanRequested_.exchange(false)) {
            if (!scan()) break;
            if (config_.live) setupWatches();
            continue;
        }

        if (inotify_ && (fds[1].revents & POLLIN)) handle(inotify_->read());
        flushStaleMoves();

        const auto now = Clock::now();
        if (dirty_ && (now - lastChange_ > kQuietTime || now - firstChange_ > kMaxDelay)) {
            dirty_ = false;
            publish(IndexSnapshot::Origin::Live);
        }
        if (unsaved_ && now - lastSave_ > kSaveEvery) save();
    }
    if (unsaved_) save();
}

bool IndexService::scan() {
    scanning_ = true;
    progress_.entries = 0;
    progress_.errors = 0;

    auto fresh = std::make_unique<Index>();
    if (!scanFilesystem(*fresh, config_.scan, &progress_)) return false;  // stopped
    fresh->shrinkToFit();
    index_ = std::move(fresh);
    if (!config_.cachePath.empty()) saveIndex(*index_, config_.cachePath, config_.source);  // already parent-first
    unsaved_ = false;
    dirty_ = false;
    lastSave_ = Clock::now();
    publish(IndexSnapshot::Origin::Scan);
    return true;
}

void IndexService::save() {
    // live updates leave tombstones and moved records out of order, the file wants neither
    if (!config_.cachePath.empty()) saveIndex(index_->compacted(), config_.cachePath, config_.source);
    unsaved_ = false;
    lastSave_ = Clock::now();
}

void IndexService::publish(IndexSnapshot::Origin origin, int64_t cachedAt) {
    auto copy = std::make_unique<Index>(*index_);  // ~25MB memcpy for 500k files, the builder does the slow part
    {
        std::lock_guard lock(jobMutex_);
        job_ = Job{std::move(copy), origin, cachedAt};  // newest wins, an unbuilt older one is just dropped
    }
    jobCv_.notify_one();
}

// --------------------------------------------------------------------------------------------- builder

void IndexService::buildLoop() {
    for (;;) {
        Job job;
        {
            std::unique_lock lock(jobMutex_);
            jobCv_.wait(lock, [&] { return stopBuilder_ || job_; });
            if (stopBuilder_) return;
            job = std::move(*job_);
            job_.reset();
        }
        auto snapshot = std::make_unique<IndexSnapshot>();
        snapshot->index = std::move(job.index);
        snapshot->paths.build(*snapshot->index);
        snapshot->sorts.build(*snapshot->index, snapshot->paths);
        snapshot->origin = job.origin;
        snapshot->cachedAt = job.cachedAt;
        for (RecordId id = 0; id < snapshot->index->size(); ++id)
            snapshot->liveCount += !snapshot->index->isDeleted(id);
        {
            std::lock_guard lock(snapshotMutex_);
            snapshot_ = std::move(snapshot);
        }
        if (job.origin == IndexSnapshot::Origin::Scan) scanning_ = false;
        if (wake_) wake_();
        // sorting churns through a lot of temp memory on this thread, glibc keeps it in a per-thread
        // arena otherwise. ~100MB difference in rss
        malloc_trim(0);
    }
}

// ------------------------------------------------------------------------------------------------ live

void IndexService::setupWatches() {
    inotify_ = std::make_unique<Inotify>();
    wdToDir_.clear();
    dirToWd_.clear();
    extraChildren_.clear();
    pendingMoves_.clear();
    buildChildren();
    watched_ = 0;
    overLimit_ = 0;
    if (!inotify_->ok()) return;

    PathCache paths;
    paths.build(*index_);
    std::vector<RecordId> dirs;
    for (RecordId id = 0; id < index_->size(); ++id) {
        const FileRecord& r = index_->record(id);
        if ((r.flags & kIsDir) && !(r.flags & kIsDeleted)) dirs.push_back(id);
    }
    totalDirs_ = dirs.size();

    // every app of this user shares the watch limit, so take at most half.
    // if that's not enough, home goes first (that's where stuff changes), /usr last
    const char* home = std::getenv("HOME");
    const std::string homeDir = home ? std::string(home) + "/" : std::string("\x01");
    auto rank = [&](RecordId id) {
        const std::string_view p = paths.dirPath(id);
        if (p.starts_with(homeDir) || p == std::string_view(homeDir).substr(0, homeDir.size() - 1)) return 0;
        return p.starts_with("/usr/") ? 2 : 1;
    };
    std::stable_sort(dirs.begin(), dirs.end(), [&](RecordId a, RecordId b) { return rank(a) < rank(b); });

    watchBudget_ = Inotify::maxUserWatches() / 2;
    for (RecordId id : dirs) addWatch(id, std::string(paths.dirPath(id)));
    watched_ = wdToDir_.size();
}

void IndexService::addWatch(RecordId dir, const std::string& path) {
    if (!inotify_) return;
    if (wdToDir_.size() >= watchBudget_) {
        ++overLimit_;
        return;
    }
    const int wd = inotify_->addWatch(path);
    if (wd < 0) return;  // no permission, or gone already
    wdToDir_[wd] = dir;
    dirToWd_[dir] = wd;
    watched_ = wdToDir_.size();
}

void IndexService::buildChildren() {
    const size_t n = index_->size();
    childCount_ = n;
    childStart_.assign(n + 1, 0);
    children_.resize(n);
    for (RecordId id = 0; id < n; ++id) {
        const RecordId p = index_->record(id).parent;
        if (p != kNoParent) ++childStart_[p + 1];
    }
    for (size_t i = 0; i < n; ++i) childStart_[i + 1] += childStart_[i];
    std::vector<uint32_t> fill(childStart_.begin(), childStart_.end() - 1);
    for (RecordId id = 0; id < n; ++id) {
        const RecordId p = index_->record(id).parent;
        if (p != kNoParent) children_[fill[p]++] = id;
    }
}

// children = csr entries from the last build + anything added / moved in since.
// both can be stale (moved away, deleted), so check the record itself
template <typename Fn>
void IndexService::forEachChild(RecordId dir, Fn fn) const {
    auto visit = [&](RecordId c) {
        const FileRecord& r = index_->record(c);
        if (r.parent == dir && !(r.flags & kIsDeleted)) fn(c);
    };
    if (dir < childCount_) {
        for (uint32_t i = childStart_[dir]; i < childStart_[dir + 1]; ++i) visit(children_[i]);
    }
    if (auto it = extraChildren_.find(dir); it != extraChildren_.end()) {
        for (RecordId c : it->second) visit(c);
    }
}

RecordId IndexService::findChild(RecordId dir, std::string_view name) const {
    RecordId found = kNoParent;
    forEachChild(dir, [&](RecordId c) {
        if (found == kNoParent && index_->name(c) == name) found = c;
    });
    return found;
}

std::string IndexService::childPath(RecordId dir, std::string_view name) const {
    std::string path = index_->fullPath(dir);
    if (path.back() != '/') path += '/';
    path += name;
    return path;
}

void IndexService::markChanged() {
    const auto now = Clock::now();
    if (!dirty_) firstChange_ = now;
    dirty_ = unsaved_ = true;
    lastChange_ = now;
    ++changes_;
}

void IndexService::handle(const std::vector<Inotify::Event>& events) {
    for (const Inotify::Event& e : events) {
        if (e.mask & IN_Q_OVERFLOW) {  // kernel queue overflowed, we lost events. only a rescan fixes that
            rescanRequested_ = true;
            continue;
        }
        auto it = wdToDir_.find(e.wd);
        if (it == wdToDir_.end()) continue;
        const RecordId dir = it->second;
        if (e.mask & IN_IGNORED) {  // watch is gone (dir deleted / unmounted)
            dirToWd_.erase(dir);
            wdToDir_.erase(it);
            watched_ = wdToDir_.size();
            continue;
        }
        if (e.name.empty() || index_->isDeleted(dir)) continue;

        if (e.mask & IN_CREATE) {
            onCreated(dir, e.name);
        } else if (e.mask & IN_MOVED_FROM) {
            const RecordId c = findChild(dir, e.name);
            if (c != kNoParent) pendingMoves_[e.cookie] = {c, Clock::now()};
        } else if (e.mask & IN_MOVED_TO) {
            auto m = pendingMoves_.find(e.cookie);
            if (m != pendingMoves_.end()) {
                onMoved(m->second.id, dir, e.name);
                pendingMoves_.erase(m);
            } else {
                onCreated(dir, e.name);  // moved in from somewhere we don't watch
            }
        } else if (e.mask & IN_DELETE) {
            const RecordId c = findChild(dir, e.name);
            if (c != kNoParent) deleteTree(c);
        } else if (e.mask & (IN_CLOSE_WRITE | IN_ATTRIB)) {
            const RecordId c = findChild(dir, e.name);
            if (c != kNoParent)
                refresh(c);
            else
                onCreated(dir, e.name);
        }
    }
}

void IndexService::onCreated(RecordId dir, const std::string& name) {
    const std::string path = childPath(dir, name);
    struct stat st {};
    if (lstat(path.c_str(), &st) != 0) return;  // already gone again
    const uint8_t flags = flagsFor(st);
    const bool isDir = flags & kIsDir;

    // already known (we scanned it after adding the parent's watch, or a duplicate event)
    if (const RecordId existing = findChild(dir, name); existing != kNoParent) {
        if (bool(index_->record(existing).flags & kIsDir) == isDir) {
            refresh(existing);
            return;
        }
        deleteTree(existing);  // file replaced by a dir or the other way round
    }
    if (isDir &&
        std::find(config_.scan.excludes.begin(), config_.scan.excludes.end(), path) != config_.scan.excludes.end()) {
        return;
    }

    const RecordId id = index_->add(dir, name, flags, isDir ? 0 : uint64_t(st.st_size), st.st_mtime);
    extraChildren_[dir].push_back(id);
    markChanged();
    if (!isDir) return;

    // watch first, then scan: anything created in between shows up in the scan or as an event (or both, fine)
    addWatch(id, path);
    const size_t before = index_->size();
    scanInto(*index_, id, path, config_.scan);
    for (RecordId n = static_cast<RecordId>(before); n < index_->size(); ++n) {
        extraChildren_[index_->record(n).parent].push_back(n);
        if (index_->record(n).flags & kIsDir) addWatch(n, index_->fullPath(n));
    }
}

void IndexService::onMoved(RecordId id, RecordId newDir, const std::string& name) {
    if (index_->isDeleted(id)) return;
    // rename over an existing file (editors saving via tmp file + rename), the old one is gone
    if (const RecordId existing = findChild(newDir, name); existing != kNoParent && existing != id) {
        deleteTree(existing);
    }
    index_->move(id, newDir, name);
    extraChildren_[newDir].push_back(id);
    markChanged();  // a moved folder keeps its subtree and its watches, paths follow automatically
}

void IndexService::refresh(RecordId id) {
    struct stat st {};
    const RecordId parent = index_->record(id).parent;
    if (lstat(childPath(parent, index_->name(id)).c_str(), &st) != 0) return;
    const uint8_t flags = flagsFor(st);
    index_->setMeta(id, flags, (flags & kIsDir) ? 0 : uint64_t(st.st_size), st.st_mtime);
    markChanged();
}

void IndexService::deleteTree(RecordId id) {
    std::vector<RecordId> stack{id};
    while (!stack.empty()) {
        const RecordId cur = stack.back();
        stack.pop_back();
        if (index_->isDeleted(cur)) continue;
        if (index_->record(cur).flags & kIsDir) {
            forEachChild(cur, [&](RecordId c) { stack.push_back(c); });
            // moved out of our sight: the dir still exists, its watch would keep eating the budget
            if (auto w = dirToWd_.find(cur); w != dirToWd_.end() && inotify_) {
                inotify_->removeWatch(w->second);
                wdToDir_.erase(w->second);
                dirToWd_.erase(w);
            }
        }
        index_->markDeleted(cur);
    }
    watched_ = wdToDir_.size();
    markChanged();
}

void IndexService::flushStaleMoves() {
    const auto now = Clock::now();
    for (auto it = pendingMoves_.begin(); it != pendingMoves_.end();) {
        if (now - it->second.when > kMoveTimeout) {
            deleteTree(it->second.id);
            it = pendingMoves_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace fe
