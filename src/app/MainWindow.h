#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/IconCache.h"
#include "app/IndexService.h"
#include "app/PreferencesDialog.h"
#include "app/Settings.h"
#include "app/UpdateChecker.h"
#include "core/Index.h"
#include "core/PathCache.h"
#include "core/Query.h"
#include "core/Sort.h"
#include "platform/AppBus.h"
#include "platform/Appearance.h"
#include "platform/Scanner.h"
#include "platform/Tray.h"

namespace fe {

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
    void rescan();
    void setQuery(const std::string& text);       // --search, like Everything's -search
    void remote(const AppBus::Request& request);  // another launch / the global shortcut
    void tick();                                  // every loop iteration, even while the window is hidden
    void draw();                                  // only while visible

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
    void startIndexService(bool scanNow);
    void applyPreferences(Settings next);
    void adoptIndex(std::unique_ptr<IndexSnapshot> snapshot);
    void runSearch(bool keepSelection = false);
    void applySort();
    void handleKeyboard();
    void drawMenuBar();
    void drawResultsTable();
    void drawRowContextMenu(RecordId id);
    void drawStatusBar();
    void drawHelpPopups();

    // selection: a bitmap by record id (survives sorting + live updates), the cursor row for the keyboard,
    // the anchor row for shift ranges. nothing marked but a cursor = the cursor row is the selection
    bool isMarked(RecordId id) const { return id < marked_.size() * 64 && ((marked_[id >> 6] >> (id & 63)) & 1); }
    void setMark(RecordId id, bool on);
    void clearMarks();
    void selectOnly(int row);
    void selectRange(int from, int to);
    void toggleRow(int row);
    void selectAll();
    std::vector<RecordId> selectedIds() const;  // in result order
    void requestTrash();
    void moveSelectionToTrash();
    void openIds(const std::vector<RecordId>& ids);

    void openSelected();
    void revealSelected();
    void copySelectedPath();
    void copySelectedName();
    void setZoom(float zoom);
    void setStatus(std::string message);

    // the ui's copy, swapped in whole from IndexService snapshots
    std::unique_ptr<Index> index_;
    PathCache paths_;
    SortOrders sorts_;
    size_t objectCount_ = 0;
    std::unique_ptr<IndexService> service_;
    std::function<void()> wake_;
    std::chrono::steady_clock::time_point scanStart_;
    double scanSeconds_ = 0;
    int64_t cachedIndexTime_ = 0;
    UpdateChecker updates_;

    char queryBuf_[512] = {};
    SearchOptions searchOptions_;
    bool focusSearch_ = true;
    bool queryReplaced_ = false;     // setQuery() from outside, the input needs to reload
    bool cursorToEnd_ = false;       // ...and the cursor goes to the end instead of select-all
    std::vector<RecordId> results_;  // already sorted
    double searchMs_ = 0;
    std::string queryError_;  // bad regex / size / date in the query
    SortKey sortKey_ = SortKey::Name;
    bool sortDescending_ = false;

    int selectedRow_ = -1;  // cursor, into results_
    int anchorRow_ = -1;
    std::vector<uint64_t> marked_;
    size_t markedCount_ = 0;
    std::vector<RecordId> pendingIds_;  // waiting for "are you sure" (trash / open many)
    bool openTrashConfirm_ = false, openManyConfirm_ = false;
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
    WindowRequest windowRequest_ = WindowRequest::None;
    bool waitingForTray_ = false;  // started hidden, tray not up yet
    std::chrono::steady_clock::time_point startTime_;

    bool openSyntaxHelp_ = false;
    bool openAbout_ = false;
    bool openUpdates_ = false;
    bool quit_ = false;

    std::string statusMessage_;
    std::chrono::steady_clock::time_point statusTime_;
};

}  // namespace fe
