#include "app/MainWindow.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <utility>

#include "AppIcon.h"
#include "app/Theme.h"
#include "core/IndexFile.h"
#include "core/Search.h"
#include "imgui.h"
#include "imgui_internal.h"  // ClearActiveID
#include "platform/Autostart.h"
#include "platform/GlobalShortcut.h"
#include "platform/Paths.h"
#include "platform/Shell.h"
#include "platform/Trash.h"
#include "stb_image.h"

#ifndef FE_VERSION
#define FE_VERSION "dev"
#endif

namespace fe {

namespace {

std::string formatSize(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        ++u;
    }
    char buf[32];
    std::snprintf(buf, sizeof buf, u == 0 ? "%.0f %s" : "%.1f %s", v, units[u]);
    return buf;
}

std::string formatTime(int64_t unixSeconds) {
    std::time_t t = static_cast<std::time_t>(unixSeconds);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
    return buf;
}

double msSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

std::string formatAge(int64_t unixSeconds) {
    const int64_t now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const int64_t s = std::max<int64_t>(0, now - unixSeconds);
    char buf[32];
    if (s < 60)
        std::snprintf(buf, sizeof buf, "just now");
    else if (s < 3600)
        std::snprintf(buf, sizeof buf, "%lld min ago", static_cast<long long>(s / 60));
    else if (s < 86400)
        std::snprintf(buf, sizeof buf, "%lld h ago", static_cast<long long>(s / 3600));
    else
        std::snprintf(buf, sizeof buf, "%lld days ago", static_cast<long long>(s / 86400));
    return buf;
}

// fallback when the icon theme has nothing
void drawIcon(ImVec2 pos, float lineHeight, bool isDir, ImU32 color) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = lineHeight * 0.85f;
    const ImVec2 o(pos.x, pos.y + (lineHeight - s) * 0.5f);
    if (isDir) {
        dl->AddRectFilled(ImVec2(o.x, o.y + s * 0.12f), ImVec2(o.x + s * 0.45f, o.y + s * 0.35f), color, s * 0.08f);
        dl->AddRectFilled(ImVec2(o.x, o.y + s * 0.25f), ImVec2(o.x + s, o.y + s * 0.88f), color, s * 0.1f);
    } else {
        const float w = s * 0.72f, x = o.x + (s - w) * 0.5f;
        dl->AddRect(ImVec2(x, o.y + s * 0.05f), ImVec2(x + w, o.y + s * 0.95f), color, s * 0.1f, 0, 1.2f);
        for (int i = 0; i < 3; ++i) {
            const float y = o.y + s * (0.35f + 0.18f * i);
            dl->AddLine(ImVec2(x + w * 0.22f, y), ImVec2(x + w * 0.78f, y), color, 1.0f);
        }
    }
}

// app icon as ARGB32 (network order) at a few sizes, the panel picks one
std::vector<Tray::Icon> trayIcons() {
    int w, h, channels;
    uint8_t* rgba = stbi_load_from_memory(kAppIconPng, sizeof kAppIconPng, &w, &h, &channels, 4);
    if (!rgba) return {};
    std::vector<Tray::Icon> icons;
    for (int size : {w, 64, 32}) {
        const int step = w / size;  // 256 -> 64/32 is an exact box filter
        if (step < 1 || w % size != 0 || h != w) continue;
        Tray::Icon icon{size, size, std::vector<uint8_t>(static_cast<size_t>(size) * size * 4)};
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                unsigned sum[4] = {0, 0, 0, 0};
                for (int dy = 0; dy < step; ++dy) {
                    for (int dx = 0; dx < step; ++dx) {
                        const uint8_t* p = rgba + ((y * step + dy) * w + (x * step + dx)) * 4;
                        for (int c = 0; c < 4; ++c) sum[c] += p[c];
                    }
                }
                uint8_t* out = &icon.argb[(static_cast<size_t>(y) * size + x) * 4];
                const unsigned n = static_cast<unsigned>(step * step);
                out[0] = static_cast<uint8_t>(sum[3] / n);  // A R G B
                out[1] = static_cast<uint8_t>(sum[0] / n);
                out[2] = static_cast<uint8_t>(sum[1] / n);
                out[3] = static_cast<uint8_t>(sum[2] / n);
            }
        }
        icons.push_back(std::move(icon));
    }
    stbi_image_free(rgba);
    return icons;
}

}  // namespace

MainWindow::MainWindow(const SystemAppearance& system, float displayScale, std::function<void()> wake)
    : wake_(std::move(wake)), systemAppearance_(system), displayScale_(displayScale), settings_(loadSettings()) {
    applyTheme(appearance(), displayScale_);
    updateTray();
}

void MainWindow::updateTray() {
    if (settings_.trayIcon && !tray_) tray_ = std::make_unique<Tray>(trayIcons(), wake_);
    if (!settings_.trayIcon) tray_.reset();
}

void MainWindow::rememberWindowGeometry(const WindowGeometry& geometry) {
    if (geometry == settings_.window) return;
    settings_.window = geometry;
    saveSettings(settings_);
}

bool MainWindow::hideOnClose() const {
    return settings_.trayIcon && settings_.closeToTray && tray_ && tray_->available();
}

MainWindow::WindowRequest MainWindow::takeWindowRequest() { return std::exchange(windowRequest_, WindowRequest::None); }

SystemAppearance MainWindow::appearance() const {
    SystemAppearance a = systemAppearance_;
    if (settings_.theme == Settings::Theme::Dark) a.dark = true;
    if (settings_.theme == Settings::Theme::Light) a.dark = false;
    if (settings_.customAccent) {
        a.hasAccent = true;
        std::copy(std::begin(settings_.accent), std::end(settings_.accent), a.accent);
    }
    return a;
}

MainWindow::~MainWindow() = default;  // service_ stops + saves in its destructor

void MainWindow::startIndexService(bool scanNow) {
    service_.reset();  // old one stops first, it might still be saving
    IndexServiceConfig config;
    config.scan = scanOptionsFor(settings_);
    config.source = scanSourceFor(config.scan, settings_);
    config.cachePath = indexCachePath();
    config.scanOnStart = scanNow;
    config.live = settings_.liveUpdates;
    scanStart_ = std::chrono::steady_clock::now();
    service_ = std::make_unique<IndexService>(std::move(config), wake_);
}

void MainWindow::rescan() {
    if (!service_ || service_->scanning()) return;
    scanStart_ = std::chrono::steady_clock::now();
    service_->rescan();
}

void MainWindow::start(bool background) {
    startIndexService(settings_.rescanOnStartup);

    startTime_ = std::chrono::steady_clock::now();
    if (background && tray_ && settings_.startInTray)
        waitingForTray_ = true;  // stay hidden
    else
        windowRequest_ = background ? WindowRequest::Minimize : WindowRequest::Show;
}

void MainWindow::setQuery(const std::string& text) {
    text.copy(queryBuf_, sizeof queryBuf_ - 1);
    queryBuf_[std::min(text.size(), sizeof queryBuf_ - 1)] = '\0';
    runSearch();  // no-op until the index is in, adoptIndex searches again anyway
    queryReplaced_ = true;
    cursorToEnd_ = true;
}

void MainWindow::remote(const AppBus::Request& request) {
    if (!request.search.empty()) setQuery(request.search);
    windowRequest_ = request.kind == AppBus::Request::Kind::Toggle ? WindowRequest::Toggle : WindowRequest::Show;
    waitingForTray_ = false;
}

void MainWindow::tick() {
    if (auto snapshot = service_ ? service_->takeSnapshot() : nullptr) {
        if (snapshot->origin == IndexSnapshot::Origin::Cache) cachedIndexTime_ = snapshot->cachedAt;
        if (snapshot->origin == IndexSnapshot::Origin::Scan) {
            cachedIndexTime_ = 0;
            scanSeconds_ = msSince(scanStart_) / 1000.0;
        }
        objectCount_ = snapshot->liveCount;
        adoptIndex(std::move(snapshot));
    }
    if (updates_.takeResult()) openUpdates_ = true;

    if (tray_) {
        for (Tray::Action action : tray_->takeActions()) {
            switch (action) {
                case Tray::Action::Toggle:
                    windowRequest_ = WindowRequest::Toggle;
                    break;
                case Tray::Action::Show:
                    windowRequest_ = WindowRequest::Show;
                    break;
                case Tray::Action::Rescan:
                    rescan();
                    break;
                case Tray::Action::Quit:
                    quit_ = true;
                    break;
            }
        }
    }
    if (windowRequest_ == WindowRequest::Show || windowRequest_ == WindowRequest::Toggle) focusSearch_ = true;

    // started hidden but the panel never picked up our icon, don't stay invisible forever
    if (waitingForTray_) {
        if (tray_ && tray_->available()) {
            waitingForTray_ = false;
        } else if (std::chrono::steady_clock::now() - startTime_ > std::chrono::seconds(30)) {
            waitingForTray_ = false;
            windowRequest_ = WindowRequest::Minimize;
        }
    }
}

void MainWindow::adoptIndex(std::unique_ptr<IndexSnapshot> snapshot) {
    // live updates keep record ids, so the selection just stays. anything else renumbers everything:
    // keep the cursor row by path, drop the rest
    if (snapshot->origin == IndexSnapshot::Origin::Live && index_) {
        index_ = std::move(snapshot->index);
        paths_ = std::move(snapshot->paths);
        sorts_ = std::move(snapshot->sorts);
        runSearch(/*keepSelection=*/true);
        return;
    }
    std::string selectedPath = (index_ && selectedRow_ >= 0) ? index_->fullPath(results_[selectedRow_]) : "";

    index_ = std::move(snapshot->index);
    paths_ = std::move(snapshot->paths);
    sorts_ = std::move(snapshot->sorts);
    runSearch();

    if (selectedPath.empty()) return;
    std::string_view selectedName = std::string_view(selectedPath).substr(selectedPath.find_last_of('/') + 1);
    for (size_t i = 0; i < results_.size(); ++i) {
        if (index_->name(results_[i]) == selectedName && index_->fullPath(results_[i]) == selectedPath) {
            selectOnly(static_cast<int>(i));
            break;
        }
    }
}

bool MainWindow::consumeFontRebuild() {
    const bool dirty = fontsDirty_;
    fontsDirty_ = false;
    return dirty;
}

void MainWindow::setZoom(float zoom) {
    zoom = std::clamp(zoom, 0.7f, 2.0f);
    if (zoom == settings_.fontZoom) return;
    settings_.fontZoom = zoom;
    fontsDirty_ = true;
    saveSettings(settings_);
}

void MainWindow::applyPreferences(Settings next) {
    if (next.launchAtLogin != settings_.launchAtLogin && !setLaunchAtLogin(next.launchAtLogin)) {
        setStatus("Could not change the login startup entry");
        next.launchAtLogin = settings_.launchAtLogin;
    }
    const bool indexChanged =
        next.excludedVolumes != settings_.excludedVolumes || next.excludedFolders != settings_.excludedFolders;
    const bool liveChanged = next.liveUpdates != settings_.liveUpdates;
    const bool restyle = next.theme != settings_.theme || next.customAccent != settings_.customAccent ||
                         !std::equal(std::begin(next.accent), std::end(next.accent), std::begin(settings_.accent));
    const bool refont = restyle || next.fontZoom != settings_.fontZoom;

    const bool shortcutChanged =
        next.globalShortcut != settings_.globalShortcut || (next.globalShortcut && next.shortcut != settings_.shortcut);
    if (shortcutChanged) {
        if (!setGlobalShortcut(next.globalShortcut ? next.shortcut : "")) {
            setStatus("Could not change the global shortcut");
            next.globalShortcut = settings_.globalShortcut;
            next.shortcut = settings_.shortcut;
        } else if (next.globalShortcut) {
            const std::string conflict = shortcutConflict(next.shortcut);
            setStatus(conflict.empty() ? prettyAccel(next.shortcut) + " now opens FindEverything"
                                       : prettyAccel(next.shortcut) + " is also used by \"" + conflict +
                                             "\", pick another one if it doesn't work");
        }
    }

    next.window = settings_.window;  // the dialog's copy is stale
    settings_ = std::move(next);
    if (!saveSettings(settings_)) setStatus("Could not save settings to " + settingsFilePath());
    updateTray();
    if (restyle) applyTheme(appearance(), displayScale_);
    if (refont) fontsDirty_ = true;
    if (indexChanged)
        startIndexService(true);
    else if (liveChanged)
        startIndexService(false);  // the old service saved on its way out, cache is current
}

void MainWindow::runSearch(bool keepSelection) {
    if (!index_) return;
    const RecordId cursorId = (keepSelection && selectedRow_ >= 0) ? results_[selectedRow_] : kNoParent;
    const RecordId anchorId = (keepSelection && anchorRow_ >= 0) ? results_[anchorRow_] : kNoParent;
    auto t0 = std::chrono::steady_clock::now();
    const Query query = parseQuery(queryBuf_, searchOptions_);
    queryError_ = query.error;
    std::vector<RecordId> matches = search(*index_, query, 0, &paths_);
    results_ = sortResults(matches, index_->size(), sorts_, sortKey_, sortDescending_);
    searchMs_ = msSince(t0);
    selectedRow_ = anchorRow_ = -1;
    if (!keepSelection) {
        clearMarks();
        return;
    }
    // live updates land all the time, a shift-range in progress has to survive them
    auto rowOf = [&](RecordId id) {
        auto it = std::find(results_.begin(), results_.end(), id);
        return it == results_.end() ? -1 : static_cast<int>(it - results_.begin());
    };
    if (cursorId != kNoParent) selectedRow_ = rowOf(cursorId);
    if (anchorId != kNoParent) anchorRow_ = rowOf(anchorId);
    // marks of records that got deleted just stop showing up, recount what's left
    markedCount_ = 0;
    for (RecordId id : results_) markedCount_ += isMarked(id);
}

void MainWindow::setMark(RecordId id, bool on) {
    if (id >= marked_.size() * 64) marked_.resize(index_->size() / 64 + 1, 0);
    const uint64_t bit = uint64_t{1} << (id & 63);
    const bool was = marked_[id >> 6] & bit;
    if (was == on) return;
    marked_[id >> 6] ^= bit;
    markedCount_ += on ? 1 : -1;
}

void MainWindow::clearMarks() {
    std::fill(marked_.begin(), marked_.end(), 0);
    markedCount_ = 0;
}

void MainWindow::selectOnly(int row) {
    clearMarks();
    selectedRow_ = anchorRow_ = row;
    if (row >= 0) setMark(results_[row], true);
}

void MainWindow::selectRange(int from, int to) {
    clearMarks();
    for (int r = std::min(from, to); r <= std::max(from, to); ++r) setMark(results_[r], true);
    selectedRow_ = to;
}

void MainWindow::toggleRow(int row) {
    setMark(results_[row], !isMarked(results_[row]));
    selectedRow_ = anchorRow_ = row;
}

void MainWindow::selectAll() {
    if (results_.empty()) return;
    clearMarks();
    for (RecordId id : results_) setMark(id, true);
    if (selectedRow_ < 0) selectedRow_ = anchorRow_ = 0;
}

std::vector<RecordId> MainWindow::selectedIds() const {
    std::vector<RecordId> ids;
    if (!index_) return ids;
    if (markedCount_ == 0) {
        if (selectedRow_ >= 0) ids.push_back(results_[selectedRow_]);
        return ids;
    }
    ids.reserve(markedCount_);
    for (RecordId id : results_) {
        if (isMarked(id)) ids.push_back(id);
    }
    return ids;
}

void MainWindow::openIds(const std::vector<RecordId>& ids) {
    size_t failed = 0;
    for (RecordId id : ids) failed += !openPath(index_->fullPath(id));
    if (failed) setStatus("Could not open " + std::to_string(failed) + " of " + std::to_string(ids.size()) + " items");
}

void MainWindow::requestTrash() {
    pendingIds_.clear();
    for (RecordId id : selectedIds()) {
        if (index_->record(id).parent != kNoParent) pendingIds_.push_back(id);  // never a whole disk
    }
    if (!pendingIds_.empty()) openTrashConfirm_ = true;
}

void MainWindow::moveSelectionToTrash() {
    size_t moved = 0;
    std::string firstError;
    for (RecordId id : pendingIds_) {
        const std::string path = index_->fullPath(id);
        std::string error;
        if (moveToTrash(path, &error))
            ++moved;
        else if (firstError.empty())
            firstError = path + ": " + error;
    }
    const size_t failed = pendingIds_.size() - moved;
    pendingIds_.clear();
    clearMarks();
    std::string msg = "Moved " + std::to_string(moved) + (moved == 1 ? " item" : " items") + " to the Trash";
    if (failed)
        msg += ", " + std::to_string(failed) + " failed (" + firstError + ")";
    else if (!settings_.liveUpdates)
        msg += ". Press F5 to update the results";
    setStatus(msg);
}

void MainWindow::applySort() {
    if (!index_) return;
    // marks are by id so they survive, the cursor and anchor rows have to be found again
    const RecordId cursorId = selectedRow_ >= 0 ? results_[selectedRow_] : kNoParent;
    results_ = sortResults(results_, index_->size(), sorts_, sortKey_, sortDescending_);
    selectedRow_ = anchorRow_ = -1;
    if (cursorId != kNoParent) {
        auto it = std::find(results_.begin(), results_.end(), cursorId);
        if (it != results_.end()) selectedRow_ = anchorRow_ = static_cast<int>(it - results_.begin());
        scrollToSelected_ = true;
    }
}

void MainWindow::setStatus(std::string message) {
    statusMessage_ = std::move(message);
    statusTime_ = std::chrono::steady_clock::now();
}

void MainWindow::openSelected() {
    const std::vector<RecordId> ids = selectedIds();
    if (ids.size() > 10) {  // a stray Enter on 500 selected files would be no fun
        pendingIds_ = ids;
        openManyConfirm_ = true;
        return;
    }
    openIds(ids);
}

void MainWindow::revealSelected() {
    std::vector<std::string> paths;
    for (RecordId id : selectedIds()) {
        if (paths.size() == 100) break;
        paths.push_back(index_->fullPath(id));
    }
    if (!paths.empty() && !revealInFileManager(paths)) setStatus("Could not open the file manager");
}

void MainWindow::copySelectedPath() {
    const std::vector<RecordId> ids = selectedIds();
    if (ids.empty()) return;
    std::string text;
    for (RecordId id : ids) text += (text.empty() ? "" : "\n") + index_->fullPath(id);
    ImGui::SetClipboardText(text.c_str());
    setStatus(ids.size() == 1 ? "Copied full path" : "Copied " + std::to_string(ids.size()) + " paths");
}

void MainWindow::copySelectedName() {
    const std::vector<RecordId> ids = selectedIds();
    if (ids.empty()) return;
    std::string text;
    for (RecordId id : ids) text += (text.empty() ? "" : "\n") + std::string(index_->name(id));
    ImGui::SetClipboardText(text.c_str());
    setStatus(ids.size() == 1 ? "Copied name" : "Copied " + std::to_string(ids.size()) + " names");
}

void MainWindow::draw() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("FindEverything", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_MenuBar);  // saved settings on, that's what keeps column widths in imgui.ini

    drawMenuBar();
    handleKeyboard();

    // filter dropdown, then the search box filling the rest of the line
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    if (ImGui::BeginCombo("##filter", filterName(searchOptions_.filter))) {
        for (int f = 0; f < static_cast<int>(Filter::Count); ++f) {
            const auto filter = static_cast<Filter>(f);
            if (ImGui::Selectable(filterName(filter), searchOptions_.filter == filter)) {
                searchOptions_.filter = filter;
                runSearch();
                focusSearch_ = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    // an active InputText edits its own copy and ignores changes to our buffer, so deactivate it
    // and focus it again, that makes imgui reload the text
    if (queryReplaced_) {
        ImGui::ClearActiveID();
        focusSearch_ = true;
        queryReplaced_ = false;
    }
    if (focusSearch_) {
        ImGui::SetKeyboardFocusHere();
        focusSearch_ = false;
    }
    // focusing selects all by default, after setQuery() we want the cursor at the end instead
    auto putCursorAtEnd = [](ImGuiInputTextCallbackData* data) {
        auto* self = static_cast<MainWindow*>(data->UserData);
        if (self->cursorToEnd_) {
            data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen;
            self->cursorToEnd_ = false;
        }
        return 0;
    };
    if (ImGui::InputTextWithHint("##query", "Search  (e.g.  report ext:pdf  *.cpp  size:>10mb  dm:today)", queryBuf_,
                                 sizeof queryBuf_, ImGuiInputTextFlags_CallbackAlways, putCursorAtEnd, this)) {
        runSearch();  // every keystroke, like Everything
    }

    drawResultsTable();
    if (showStatusBar_) drawStatusBar();
    drawHelpPopups();
    if (preferences_.draw() == PreferencesDialog::Result::Ok) applyPreferences(preferences_.edited());
    ImGui::End();
}

void MainWindow::drawMenuBar() {
    if (!ImGui::BeginMenuBar()) return;
    const bool hasSelection = index_ && (selectedRow_ >= 0 || markedCount_ > 0);
    const bool many = markedCount_ > 1;

    // shortcut labels are just text here, the keys live in handleKeyboard()
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open", "Enter", false, hasSelection)) openSelected();
        if (ImGui::MenuItem("Open Containing Folder", "Ctrl+Enter", false, hasSelection)) revealSelected();
        if (ImGui::MenuItem("Move to Trash", "Delete", false, hasSelection)) requestTrash();
        ImGui::Separator();
        if (ImGui::MenuItem("Rescan", "F5", false, service_ && !service_->scanning())) rescan();
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Ctrl+Q")) quit_ = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem(many ? "Copy Full Paths" : "Copy Full Path", "Ctrl+Shift+C", false, hasSelection))
            copySelectedPath();
        if (ImGui::MenuItem(many ? "Copy Names" : "Copy Name", nullptr, false, hasSelection)) copySelectedName();
        ImGui::Separator();
        if (ImGui::MenuItem("Select All", "Ctrl+A", false, index_ && !results_.empty())) selectAll();
        ImGui::Separator();
        if (ImGui::MenuItem("Preferences...", "Ctrl+,")) preferences_.open(settings_);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        columnsChanged_ |= ImGui::MenuItem("Path Column", nullptr, &columnVisible_[kPath]);
        columnsChanged_ |= ImGui::MenuItem("Size Column", nullptr, &columnVisible_[kSize]);
        columnsChanged_ |= ImGui::MenuItem("Date Modified Column", nullptr, &columnVisible_[kModified]);
        ImGui::MenuItem("Status Bar", nullptr, &showStatusBar_);
        ImGui::Separator();
        if (ImGui::MenuItem("Zoom In", "Ctrl+=")) setZoom(settings_.fontZoom + 0.1f);
        if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) setZoom(settings_.fontZoom - 0.1f);
        if (ImGui::MenuItem("Reset Zoom", "Ctrl+0")) setZoom(1.0f);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Search")) {
        bool changed = false;
        changed |= ImGui::MenuItem("Match Case", "Ctrl+I", &searchOptions_.matchCase);
        changed |= ImGui::MenuItem("Match Whole Word", "Ctrl+B", &searchOptions_.wholeWord);
        changed |= ImGui::MenuItem("Match Path", "Ctrl+U", &searchOptions_.matchPath);
        ImGui::Separator();
        using Kind = SearchOptions::Kind;
        static const std::pair<Kind, const char*> kKinds[] = {
            {Kind::All, "Everything"}, {Kind::Files, "Files Only"}, {Kind::Folders, "Folders Only"}};
        for (const auto& [kind, label] : kKinds) {
            if (ImGui::MenuItem(label, nullptr, searchOptions_.kind == kind)) {
                searchOptions_.kind = kind;
                changed = true;
            }
        }
        if (changed) runSearch();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Search Syntax", "F1")) openSyntaxHelp_ = true;
        if (ImGui::MenuItem("Check for Updates...", nullptr, false, !updates_.running())) {
            updates_.start();
            setStatus("Checking for updates...");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("About FindEverything")) openAbout_ = true;
        ImGui::EndMenu();
    }

    // active search options, right aligned
    std::string active;
    if (searchOptions_.matchCase) active += "Case  ";
    if (searchOptions_.wholeWord) active += "Word  ";
    if (searchOptions_.matchPath) active += "Path  ";
    if (searchOptions_.kind == SearchOptions::Kind::Files) active += "Files  ";
    if (searchOptions_.kind == SearchOptions::Kind::Folders) active += "Folders  ";
    if (!active.empty()) {
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::CalcTextSize(active.c_str()).x -
                             ImGui::GetStyle().WindowPadding.x);
        ImGui::TextColored(themeColors().dimText, "%s", active.c_str());
    }

    ImGui::EndMenuBar();
}

void MainWindow::handleKeyboard() {
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) return;

    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Comma, false)) preferences_.open(settings_);
    const bool ctrl = ImGui::GetIO().KeyCtrl;
    const bool shift = ImGui::GetIO().KeyShift;
    auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, false); };

    if (ctrl && pressed(ImGuiKey_Q)) quit_ = true;
    if (pressed(ImGuiKey_F1)) openSyntaxHelp_ = true;
    if (pressed(ImGuiKey_F5)) rescan();
    if (ctrl && pressed(ImGuiKey_F)) focusSearch_ = true;
    if (pressed(ImGuiKey_Escape)) {  // back to the search box, multi-selection collapses to the cursor
        focusSearch_ = true;
        if (markedCount_ > 1) selectOnly(selectedRow_);
    }

    if (ctrl && (pressed(ImGuiKey_Equal) || pressed(ImGuiKey_KeypadAdd))) setZoom(settings_.fontZoom + 0.1f);
    if (ctrl && (pressed(ImGuiKey_Minus) || pressed(ImGuiKey_KeypadSubtract))) setZoom(settings_.fontZoom - 0.1f);
    if (ctrl && (pressed(ImGuiKey_0) || pressed(ImGuiKey_Keypad0))) setZoom(1.0f);

    bool optionChanged = false;
    auto toggle = [&](bool& option) {
        option = !option;
        optionChanged = true;
    };
    if (ctrl && pressed(ImGuiKey_I)) toggle(searchOptions_.matchCase);
    if (ctrl && pressed(ImGuiKey_B)) toggle(searchOptions_.wholeWord);
    if (ctrl && pressed(ImGuiKey_U)) toggle(searchOptions_.matchPath);
    if (optionChanged) runSearch();

    if (!index_) return;
    const int count = static_cast<int>(results_.size());
    auto moveSelection = [&](int delta) {
        if (count == 0) return;
        const int target = std::clamp(selectedRow_ < 0 ? 0 : selectedRow_ + delta, 0, count - 1);
        if (shift) {
            if (anchorRow_ < 0) anchorRow_ = selectedRow_ < 0 ? target : selectedRow_;
            selectRange(anchorRow_, target);
        } else {
            selectOnly(target);
        }
        scrollToSelected_ = true;
    };
    const bool typing = ImGui::IsAnyItemActive();
    const bool navigated = ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow) ||
                           ImGui::IsKeyPressed(ImGuiKey_PageDown) || ImGui::IsKeyPressed(ImGuiKey_PageUp);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) moveSelection(1);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) moveSelection(-1);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) moveSelection(20);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) moveSelection(-20);
    // arrows move focus to the list (like Everything), so delete / ctrl+a / enter act on the results
    if (navigated && typing) ImGui::ClearActiveID();

    // in the list, typing goes back to the search box
    if (!typing && !ctrl) {
        std::string typed;
        for (ImWchar c : ImGui::GetIO().InputQueueCharacters) {
            if (c < 32) continue;
            char utf8[5];
            ImTextCharToUtf8(utf8, c);
            typed += utf8;
        }
        if (!typed.empty()) {
            setQuery(queryBuf_ + typed);
        } else if (pressed(ImGuiKey_Backspace)) {
            std::string q = queryBuf_;
            while (!q.empty() && (static_cast<unsigned char>(q.back()) & 0xC0) == 0x80) q.pop_back();  // utf-8 tail
            if (!q.empty()) q.pop_back();
            setQuery(q);
        }
    }

    if (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter)) {
        if (ctrl)
            revealSelected();  // same as Everything
        else
            openSelected();
    }
    if (ctrl && shift && pressed(ImGuiKey_C)) copySelectedPath();

    // while typing, ctrl+a / delete belong to the search box. after clicking a row they're ours
    if (!ImGui::IsAnyItemActive()) {
        if (ctrl && pressed(ImGuiKey_A)) selectAll();
        if (pressed(ImGuiKey_Delete)) requestTrash();
    }
}

void MainWindow::drawResultsTable() {
    const float statusBarHeight = showStatusBar_ ? ImGui::GetFrameHeightWithSpacing() : 0.0f;
    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                  ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable |
                                  ImGuiTableFlags_Sortable;
    if (!ImGui::BeginTable("results", kColumnCount, flags, ImVec2(0, -statusBarHeight))) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    // user id = SortKey, so we know what a header click means
    const float em = ImGui::GetFontSize();
    ImGui::TableSetupColumn(
        "Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_DefaultSort,
        2.0f, static_cast<ImGuiID>(SortKey::Name));
    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 3.0f, static_cast<ImGuiID>(SortKey::Path));
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending,
                            em * 6, static_cast<ImGuiID>(SortKey::Size));
    ImGui::TableSetupColumn("Date Modified",
                            ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, em * 9,
                            static_cast<ImGuiID>(SortKey::Modified));

    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty) {
        if (specs->SpecsCount > 0) {
            sortKey_ = static_cast<SortKey>(specs->Specs[0].ColumnUserID);
            sortDescending_ = specs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
        }
        specs->SpecsDirty = false;
        applySort();
    }

    // columns can be hidden from the View menu or the header right-click menu, keep both in sync
    for (int c = kPath; c < kColumnCount; ++c) {
        if (columnsChanged_)
            ImGui::TableSetColumnEnabled(c, columnVisible_[c]);
        else
            columnVisible_[c] = ImGui::TableGetColumnFlags(c) & ImGuiTableColumnFlags_IsEnabled;
    }
    columnsChanged_ = false;

    ImGui::TableHeadersRow();

    if (index_) {
        const ThemeColors& theme = themeColors();
        const float lineHeight = ImGui::GetTextLineHeight();
        const float iconWidth = lineHeight + ImGui::GetStyle().ItemInnerSpacing.x;
        const int iconPx = static_cast<int>(lineHeight + 0.5f);
        if (!icons_ || icons_->sizePx() != iconPx) icons_ = std::make_unique<IconCache>(appearance().iconTheme, iconPx);

        // clipper = only the rows on screen get drawn, 5M results cost the same as 30
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(results_.size()));
        if (scrollToSelected_ && selectedRow_ >= 0) clipper.IncludeItemByIndex(selectedRow_);
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const RecordId id = results_[row];
                const FileRecord& r = index_->record(id);
                const bool isDir = r.flags & kIsDir;
                const bool selected = isMarked(id) || (markedCount_ == 0 && row == selectedRow_);
                const bool cursor = row == selectedRow_;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(kName);
                ImGui::PushID(row);

                // invisible full-row selectable for clicks, content drawn on top
                const ImVec2 start = ImGui::GetCursorScreenPos();
                // imgui draws a hovered row in the hover colour even when it's selected, which makes
                // a selected row look unselected under the mouse. keep it solid, just a bit brighter
                if (selected) {
                    const ImVec4 header = ImGui::GetStyleColorVec4(ImGuiCol_Header);
                    const ImVec4 brighter(std::min(header.x * 1.12f, 1.0f), std::min(header.y * 1.12f, 1.0f),
                                          std::min(header.z * 1.12f, 1.0f), header.w);
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, brighter);
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, brighter);
                }
                const bool clicked =
                    ImGui::Selectable("##row", selected,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap |
                                          ImGuiSelectableFlags_AllowDoubleClick);
                if (selected) ImGui::PopStyleColor(2);
                if (clicked) {
                    const ImGuiIO& io = ImGui::GetIO();
                    if (io.KeyCtrl)
                        toggleRow(row);
                    else if (io.KeyShift && anchorRow_ >= 0)
                        selectRange(anchorRow_, row);
                    else
                        selectOnly(row);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !io.KeyCtrl && !io.KeyShift)
                        openSelected();
                }
                if (ImGui::BeginPopupContextItem("row_menu")) {
                    if (!selected) selectOnly(row);  // right click outside the selection = select just that
                    drawRowContextMenu(id);
                    ImGui::EndPopup();
                }
                if (cursor && scrollToSelected_) {
                    ImGui::SetScrollHereY(0.5f);
                    scrollToSelected_ = false;
                }

                if (selected) ImGui::PushStyleColor(ImGuiCol_Text, theme.selectedText);
                std::string_view name = index_->name(id);
                if (ImTextureID tex = icons_->textureFor(name, r.flags, r.parent == kNoParent)) {
                    const float px = static_cast<float>(iconPx);
                    const ImVec2 p(start.x, start.y + (lineHeight - px) * 0.5f);
                    ImGui::GetWindowDrawList()->AddImage(tex, p, ImVec2(p.x + px, p.y + px));
                } else {
                    drawIcon(start, lineHeight, isDir,
                             selected ? theme.selectedText
                             : isDir  ? theme.folderIcon
                                      : theme.fileIcon);
                }
                ImGui::SetCursorScreenPos(ImVec2(start.x + iconWidth, start.y));
                ImGui::TextUnformatted(name.data(), name.data() + name.size());

                // false for hidden columns, skip the work
                if (ImGui::TableSetColumnIndex(kPath)) {
                    std::string_view dir = r.parent == kNoParent ? std::string_view() : paths_.dirPath(r.parent);
                    ImGui::TextUnformatted(dir.data(), dir.data() + dir.size());
                }
                if (ImGui::TableSetColumnIndex(kSize) && !isDir) ImGui::TextUnformatted(formatSize(r.size).c_str());
                if (ImGui::TableSetColumnIndex(kModified) && r.mtime) {
                    ImGui::TextUnformatted(formatTime(r.mtime).c_str());
                }
                if (selected) ImGui::PopStyleColor();

                ImGui::PopID();
            }
        }
    }
    ImGui::EndTable();
}

void MainWindow::drawRowContextMenu(RecordId id) {
    const bool isDir = index_->record(id).flags & kIsDir;
    const size_t n = std::max<size_t>(markedCount_, 1);
    const std::string items = n == 1 ? "" : " " + std::to_string(n) + " Items";
    if (ImGui::MenuItem(("Open" + items).c_str(), "Enter")) openSelected();
    if (ImGui::MenuItem(n > 1   ? "Show in File Manager"
                        : isDir ? "Open Parent Folder"
                                : "Open Containing Folder",
                        "Ctrl+Enter"))
        revealSelected();
    ImGui::Separator();
    if (ImGui::MenuItem(n > 1 ? "Copy Full Paths" : "Copy Full Path", "Ctrl+Shift+C")) copySelectedPath();
    if (ImGui::MenuItem(n > 1 ? "Copy Names" : "Copy Name")) copySelectedName();
    ImGui::Separator();
    if (ImGui::MenuItem(("Move" + items + " to Trash").c_str(), "Delete", false,
                        index_->record(id).parent != kNoParent))
        requestTrash();
}

void MainWindow::drawStatusBar() {
    ImGui::PushStyleColor(ImGuiCol_Text, themeColors().dimText);
    const bool showMessage = !statusMessage_.empty() && msSince(statusTime_) < 3000;
    const bool scanning = service_ && service_->scanning();
    const auto scanned = static_cast<unsigned long long>(service_ ? service_->scannedEntries() : 0);
    if (!queryError_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 90, 80, 255));
        ImGui::TextUnformatted(queryError_.c_str());
        ImGui::PopStyleColor();
    } else if (showMessage) {
        ImGui::TextUnformatted(statusMessage_.c_str());
    } else if (!index_) {
        if (scanned == 0)
            ImGui::TextUnformatted("Loading index...");
        else
            ImGui::Text("Indexing...  %llu objects", scanned);
    } else if (scanning) {
        std::string age = cachedIndexTime_ ? "index from " + formatAge(cachedIndexTime_) + ", updating" : "rescanning";
        ImGui::Text("%zu objects   ·   %zu results   ·   %s... %llu", objectCount_, results_.size(), age.c_str(),
                    scanned);
    } else if (markedCount_ > 1) {
        ImGui::Text("%zu objects   ·   %zu results   ·   %zu selected", objectCount_, results_.size(), markedCount_);
    } else if (cachedIndexTime_) {
        ImGui::Text("%zu objects   ·   %zu results   ·   search %.1f ms   ·   index from %s", objectCount_,
                    results_.size(), searchMs_, formatAge(cachedIndexTime_).c_str());
    } else {
        ImGui::Text("%zu objects   ·   %zu results   ·   search %.1f ms   ·   indexed in %.1f s", objectCount_,
                    results_.size(), searchMs_, scanSeconds_);
    }

    // live updates state, right aligned
    if (index_ && service_ && !scanning) {
        const size_t watched = service_->watchedDirs(), missed = service_->unwatchedForLimit();
        const size_t total = watched + missed;
        char live[96];
        if (!settings_.liveUpdates)
            std::snprintf(live, sizeof live, "live updates off");
        else if (watched == 0)
            std::snprintf(live, sizeof live, "live updates unavailable");
        else if (watched < total)
            std::snprintf(live, sizeof live, "live  ·  %zu of %zu folders", watched, total);
        else
            std::snprintf(live, sizeof live, "live");
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(live).x - ImGui::GetStyle().WindowPadding.x);
        ImGui::TextUnformatted(live);
        if (ImGui::IsItemHovered() && watched < total && settings_.liveUpdates) {
            ImGui::SetTooltip(
                "Only %zu of %zu folders are watched (inotify limit).\n"
                "Raise it with:\n  echo fs.inotify.max_user_watches=524288 | "
                "sudo tee /etc/sysctl.d/60-inotify.conf && sudo sysctl --system",
                watched, total);
        }
    }
    ImGui::PopStyleColor();
}

void MainWindow::drawHelpPopups() {
    // opened here and not in the menu, popup ids are per window and the menu is its own window
    if (openSyntaxHelp_) ImGui::OpenPopup("Search Syntax");
    if (openAbout_) ImGui::OpenPopup("About FindEverything");
    if (openUpdates_) ImGui::OpenPopup("Check for Updates");
    if (openTrashConfirm_) ImGui::OpenPopup("Move to Trash");
    if (openManyConfirm_) ImGui::OpenPopup("Open Many Items");
    openSyntaxHelp_ = openAbout_ = openUpdates_ = openTrashConfirm_ = openManyConfirm_ = false;

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Search Syntax", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static const char* kRows[][2] = {
            {"foo bar", "name contains foo AND bar"},
            {"\"foo bar\"", "exact phrase"},
            {"!tmp", "NOT, works with everything below too"},
            {"ext:jpg;png", "extension is jpg or png"},
            {"*.cpp   report_??.pdf", "wildcards, match the whole name"},
            {"regex:^img_[0-9]+", "regular expression (posix extended)"},
            {"size:>100mb   size:1mb..1gb", "file size, b kb mb gb tb"},
            {"dm:today  dm:week  dm:month", "date modified, also yesterday, year"},
            {"dm:2026-10-17   dm:>2026-01-01", "a day, after / before a day, a..b ranges"},
            {"file:   folder:", "only files / only folders"},
            {"src/main   path:", "match the full path instead of the name"},
        };
        if (ImGui::BeginTable("syntax", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            for (const auto& row : kRows) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row[0]);
                ImGui::TableNextColumn();
                ImGui::TextColored(themeColors().dimText, "%s", row[1]);
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
        ImGui::TextColored(themeColors().dimText,
                           "Search menu: Match Case (Ctrl+I), Whole Word (Ctrl+B), Path (Ctrl+U)");
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(ImGui::GetFontSize() * 6, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("About FindEverything", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("FindEverything %s", FE_VERSION);
        ImGui::TextColored(themeColors().dimText, "Instant file search, inspired by Everything by voidtools.");
        ImGui::TextColored(themeColors().dimText, "Built with Dear ImGui %s and GLFW. MIT licensed.", IMGUI_VERSION);
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(ImGui::GetFontSize() * 6, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Check for Updates", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        using Status = UpdateChecker::Result::Status;
        const UpdateChecker::Result r = updates_.result();
        switch (r.status) {
            case Status::UpdateAvailable:
                ImGui::Text("FindEverything %s is out.", r.latest.c_str());
                ImGui::TextColored(themeColors().dimText, "You have %s.", r.current.c_str());
                break;
            case Status::UpToDate:
                ImGui::Text("You're up to date.");
                ImGui::TextColored(themeColors().dimText, "%s is the latest release.", r.current.c_str());
                break;
            case Status::AheadOfRelease:
                ImGui::Text("You're ahead of the latest release.");
                ImGui::TextColored(themeColors().dimText, "Running %s, latest release is %s.", r.current.c_str(),
                                   r.latest.c_str());
                break;
            case Status::Failed:
                ImGui::Text("Couldn't reach GitHub.");
                ImGui::TextColored(themeColors().dimText, "Check your connection, or look on the releases page.");
                break;
        }
        ImGui::Spacing();
        const ImVec2 button(ImGui::GetFontSize() * 9, 0);
        if (ImGui::Button(r.status == Status::UpdateAvailable ? "Get the Update" : "Releases Page", button)) {
            openPath(r.url);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(ImGui::GetFontSize() * 6, 0)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // "are you sure" for the destructive / noisy bulk actions. lists a few of the items
    auto listPending = [&] {
        const size_t shown = std::min<size_t>(pendingIds_.size(), 6);
        for (size_t i = 0; i < shown; ++i) {
            ImGui::TextColored(themeColors().dimText, "  %s", index_->fullPath(pendingIds_[i]).c_str());
        }
        if (pendingIds_.size() > shown)
            ImGui::TextColored(themeColors().dimText, "  and %zu more", pendingIds_.size() - shown);
        ImGui::Spacing();
    };
    const ImVec2 button(ImGui::GetFontSize() * 8, 0);

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Move to Trash", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (pendingIds_.size() == 1)
            ImGui::Text("Move this item to the Trash?");
        else
            ImGui::Text("Move these %zu items to the Trash?", pendingIds_.size());
        listPending();
        ImGui::TextColored(themeColors().dimText, "You can restore them from the Trash in your file manager.");
        ImGui::Spacing();
        if (ImGui::Button("Move to Trash", button)) {
            moveSelectionToTrash();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", button) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            pendingIds_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Open Many Items", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Open %zu items at once?", pendingIds_.size());
        listPending();
        if (ImGui::Button("Open All", button)) {
            openIds(pendingIds_);
            pendingIds_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", button) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            pendingIds_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

}  // namespace fe
