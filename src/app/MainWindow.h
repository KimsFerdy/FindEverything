#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/IconCache.h"
#include "app/PreferencesDialog.h"
#include "app/Settings.h"
#include "core/Index.h"
#include "core/PathCache.h"
#include "core/Query.h"
#include "core/Sort.h"
#include "platform/Appearance.h"
#include "platform/Scanner.h"
#include "platform/Tray.h"

namespace fe {

// one full version of the index + everything derived from it.
// built on the scan thread, handed to the ui in one move
struct IndexSnapshot {
    std::unique_ptr<Index> index;
    PathCache paths;
    SortOrders sorts;
    int64_t cachedAt = 0;  // > 0: came from the cache file
    bool final = false;    // nothing else coming after this one
};

class MainWindow {
public:
    // wake: thread safe, pokes the main loop (glfwPostEmptyEvent)
    MainWindow(const SystemAppearance& system, float displayScale, std::function<void()> wake);
    ~MainWindow();
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    // loads the cached index, then rescans in the background.
    // background = launched at login (tray or minimized instead of popping up)
    void start(bool background);
    void startIndexing(bool loadCacheFirst = false, bool scan = true);
    void setQuery(const std::string& text);  // --search, like Everything's -search
    void tick();                             // every loop iteration, even while the window is hidden
    void draw();                             // only while visible

    // stuff main() has to do (glfw / font texture)
    enum class WindowRequest { None, Show, Hide, Toggle, Minimize };
    WindowRequest takeWindowRequest();
    bool hideOnClose() const;
    bool quitRequested() const { return quit_; }
    bool consumeFontRebuild();
    const WindowGeometry& windowGeometry() const { return settings_.window; }
    void rememberWindowGeometry(const WindowGeometry& geometry);

    // system look + user overrides
    SystemAppearance appearance() const;
    float fontScale() const { return displayScale_ * settings_.fontZoom; }

private:
    void updateTray();
    void configureScan();
    void restartIndexing();
    void applyPreferences(Settings next);
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

    // index_/paths_/sorts_ belong to the ui thread, the scan thread only touches pending_
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
    std::string cachePath_;
    ScanOptions scanOptions_;
    std::string scanSource_;
    int64_t cachedIndexTime_ = 0;

    char queryBuf_[512] = {};
    SearchOptions searchOptions_;
    bool focusSearch_ = true;
    std::vector<RecordId> results_;  // already sorted
    double searchMs_ = 0;
    SortKey sortKey_ = SortKey::Name;
    bool sortDescending_ = false;

    int selectedRow_ = -1;  // into results_
    bool scrollToSelected_ = false;

    enum Column { kName, kPath, kSize, kModified, kColumnCount };
    bool columnVisible_[kColumnCount] = {true, true, true, true};
    bool columnsChanged_ = false;
    bool showStatusBar_ = true;
    bool fontsDirty_ = false;

    SystemAppearance systemAppearance_;
    float displayScale_ = 1.0f;
    Settings settings_;
    PreferencesDialog preferences_;
    std::unique_ptr<IconCache> icons_;  // recreated when the row height changes
    std::unique_ptr<Tray> tray_;
    std::function<void()> wake_;
    WindowRequest windowRequest_ = WindowRequest::None;
    bool waitingForTray_ = false;  // started hidden, tray not up yet
    std::chrono::steady_clock::time_point startTime_;

    bool openSyntaxHelp_ = false;
    bool openAbout_ = false;
    bool quit_ = false;

    std::string statusMessage_;
    std::chrono::steady_clock::time_point statusTime_;
};

}  // namespace fe
