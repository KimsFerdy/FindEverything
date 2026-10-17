#pragma once

#include "core/Index.h"
#include "core/PathCache.h"
#include "core/Query.h"
#include "platform/Scanner.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fe {

// The whole UI: menu bar, search box, results table, status bar.
// Indexing runs on a background thread; the finished Index is handed over to the
// UI thread through pending_ (guarded by pendingMutex_).
class MainWindow {
public:
    MainWindow() = default;
    ~MainWindow();
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    void startIndexing();
    void draw();  // call once per frame between ImGui::NewFrame() and ImGui::Render()

    // Requests the main loop acts on (they involve GLFW / the font texture).
    bool quitRequested() const { return quit_; }
    float fontZoom() const { return fontZoom_; }
    bool consumeFontZoomChange();

private:
    void runSearch();
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

    // --- index (UI thread owns index_; scanner thread fills pending_)
    std::unique_ptr<Index> index_;
    PathCache paths_;
    std::unique_ptr<Index> pending_;
    std::mutex pendingMutex_;
    std::thread scanThread_;
    ScanProgress progress_;
    bool scanning_ = false;
    std::chrono::steady_clock::time_point scanStart_;
    double scanSeconds_ = 0;

    // --- search
    char queryBuf_[512] = {};
    SearchOptions searchOptions_;
    bool focusSearch_ = true;
    std::vector<RecordId> results_;
    double searchMs_ = 0;

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
