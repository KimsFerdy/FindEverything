#pragma once

#include "core/Index.h"
#include "core/PathCache.h"
#include "core/Query.h"
#include "core/Sort.h"
#include "platform/Scanner.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fe {

// One complete version of the index plus everything derived from it. Built
// entirely on the background thread, then handed to the UI in one move.
struct IndexSnapshot {
    std::unique_ptr<Index> index;
    PathCache paths;
    SortOrders sorts;
    int64_t cachedAt = 0;  // >0: loaded from the cache file (unix time); 0: fresh scan
};

// The whole UI: menu bar, search box, results table, status bar.
// Indexing runs on a background thread; the finished Index is handed over to the
// UI thread through pending_ (guarded by pendingMutex_).
class MainWindow {
public:
    MainWindow() = default;
    ~MainWindow();
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    // Loads the cached index (instant results), then rescans in the background.
    void start();
    void startIndexing(bool loadCacheFirst = false);
    void draw();  // call once per frame between ImGui::NewFrame() and ImGui::Render()

    // Requests the main loop acts on (they involve GLFW / the font texture).
    bool quitRequested() const { return quit_; }
    float fontZoom() const { return fontZoom_; }
    bool consumeFontZoomChange();

private:
    void adoptIndex(std::unique_ptr<IndexSnapshot> snapshot);
    void runSearch();
    void applySort();
    void handleKeyboard();
    void drawMenuBar();
    void drawResultsTable();
    void drawRowContextMenu(RecordId id);
    void drawStatusBar();
    void drawHelpPopups();

    void openSelected();
    void revealSelected();
    void copySelectedPath();
    void copySelectedName();
    void setZoom(float zoom);
    void setStatus(std::string message);

    // --- index (UI thread owns index_/paths_/sorts_; scanner thread fills pending_)
    std::unique_ptr<Index> index_;
    PathCache paths_;
    SortOrders sorts_;
    std::unique_ptr<IndexSnapshot> pending_;
    std::mutex pendingMutex_;
    std::thread scanThread_;
    ScanProgress progress_;
    bool scanning_ = false;
    std::chrono::steady_clock::time_point scanStart_;
    double scanSeconds_ = 0;
    std::string cachePath_;        // ~/.cache/findeverything/index.bin ("" = no caching)
    std::string scanSource_;       // roots, stored in the cache to detect config changes
    int64_t cachedIndexTime_ = 0;  // unix time of the loaded cache, 0 = fresh scan

    // --- search
    char queryBuf_[512] = {};
    SearchOptions searchOptions_;
    bool focusSearch_ = true;
    std::vector<RecordId> results_;  // matches, in display (sorted) order
    double searchMs_ = 0;
    SortKey sortKey_ = SortKey::Name;
    bool sortDescending_ = false;

    int selectedRow_ = -1;  // index into results_
    bool scrollToSelected_ = false;

    // --- view
    enum Column { kName, kPath, kSize, kModified, kColumnCount };
    bool columnVisible_[kColumnCount] = {true, true, true, true};
    bool columnsChanged_ = false;  // menu changed visibility; push it into the table
    bool showStatusBar_ = true;
    float fontZoom_ = 1.0f;
    bool fontZoomChanged_ = false;

    bool openSyntaxHelp_ = false;
    bool openAbout_ = false;
    bool quit_ = false;

    std::string statusMessage_;  // transient, e.g. "Copied path"
    std::chrono::steady_clock::time_point statusTime_;
};

}  // namespace fe
