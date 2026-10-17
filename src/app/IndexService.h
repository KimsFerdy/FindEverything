#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "core/Index.h"
#include "core/PathCache.h"
#include "core/Sort.h"
#include "platform/Inotify.h"
#include "platform/Scanner.h"

namespace fe {

// one full version of the index + everything derived from it, ready for the ui
struct IndexSnapshot {
    enum class Origin { Cache, Scan, Live };
    std::unique_ptr<Index> index;
    PathCache paths;
    SortOrders sorts;
    Origin origin = Origin::Scan;
    int64_t cachedAt = 0;  // Cache only, when the file was written
    size_t liveCount = 0;  // records minus tombstones
};

struct IndexServiceConfig {
    ScanOptions scan;
    std::string cachePath;  // "" = no cache
    std::string source;     // stored in the cache, see scanSourceFor()
    bool scanOnStart = true;
    bool live = true;
};

// owns the real index. worker thread: load cache -> scan -> watch with inotify and apply changes.
// builder thread: copies get turned into snapshots (path cache + sort orders) without blocking either side
class IndexService {
public:
    IndexService(IndexServiceConfig config, std::function<void()> wake);
    ~IndexService();  // stops and saves unsaved live changes
    IndexService(const IndexService&) = delete;
    IndexService& operator=(const IndexService&) = delete;

    void rescan();
    std::unique_ptr<IndexSnapshot> takeSnapshot();  // newest one, or null

    bool scanning() const { return scanning_; }
    uint64_t scannedEntries() const { return progress_.entries; }
    size_t watchedDirs() const { return watched_; }
    size_t totalDirs() const { return totalDirs_; }
    size_t unwatchedForLimit() const { return overLimit_; }  // not counting dirs we can't read anyway
    uint64_t liveChanges() const { return changes_; }

private:
    using Clock = std::chrono::steady_clock;
    struct Job {
        std::unique_ptr<Index> index;
        IndexSnapshot::Origin origin;
        int64_t cachedAt;
    };
    struct PendingMove {
        RecordId id;
        Clock::time_point when;
    };

    void run();
    void buildLoop();
    bool scan();
    void save();
    void publish(IndexSnapshot::Origin origin, int64_t cachedAt = 0);
    void poke();

    // live updates, worker thread only
    void setupWatches();
    void buildChildren();
    void addWatch(RecordId dir, const std::string& path);
    template <typename Fn>
    void forEachChild(RecordId dir, Fn fn) const;
    RecordId findChild(RecordId dir, std::string_view name) const;
    std::string childPath(RecordId dir, std::string_view name) const;
    void handle(const std::vector<Inotify::Event>& events);
    void onCreated(RecordId dir, const std::string& name);
    void onMoved(RecordId id, RecordId newDir, const std::string& name);
    void refresh(RecordId id);
    void deleteTree(RecordId id);
    void flushStaleMoves();
    void markChanged();

    IndexServiceConfig config_;
    std::function<void()> wake_;

    std::unique_ptr<Index> index_;
    std::unique_ptr<Inotify> inotify_;
    std::unordered_map<int, RecordId> wdToDir_;
    std::unordered_map<RecordId, int> dirToWd_;
    size_t watchBudget_ = 0;
    std::vector<uint32_t> childStart_, children_;  // csr, built for the first childCount_ records
    size_t childCount_ = 0;
    std::unordered_map<RecordId, std::vector<RecordId>> extraChildren_;  // added / moved in since
    std::unordered_map<uint32_t, PendingMove> pendingMoves_;             // inotify cookie -> record
    bool dirty_ = false, unsaved_ = false;
    Clock::time_point firstChange_, lastChange_, lastSave_;

    int wakeFd_ = -1;  // eventfd, wakes the worker's poll()
    std::atomic<bool> stop_{false};
    std::atomic<bool> rescanRequested_{false};
    std::atomic<bool> scanning_{true};
    std::atomic<size_t> watched_{0};
    std::atomic<size_t> totalDirs_{0};
    std::atomic<size_t> overLimit_{0};
    std::atomic<uint64_t> changes_{0};
    ScanProgress progress_;

    std::mutex jobMutex_;
    std::condition_variable jobCv_;
    std::optional<Job> job_;
    bool stopBuilder_ = false;

    std::mutex snapshotMutex_;
    std::unique_ptr<IndexSnapshot> snapshot_;

    std::thread worker_;
    std::thread builder_;
};

}  // namespace fe
