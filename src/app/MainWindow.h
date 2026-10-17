#pragma once

#include "core/Index.h"
#include "platform/Scanner.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fe {

// The whole UI: search box, results table, status bar.
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

private:
    void runSearch();
    void handleKeyboard();
    void drawResultsTable();
    void drawRowContextMenu(RecordId id);
    void drawStatusBar();

    void openSelected();
    void revealSelected();
    void copySelectedPath();
    void setStatus(std::string message);

    std::unique_ptr<Index> index_;  // owned by the UI thread
    std::unique_ptr<Index> pending_;
    std::mutex pendingMutex_;
    std::thread scanThread_;
    ScanProgress progress_;
    std::chrono::steady_clock::time_point scanStart_;
    double scanSeconds_ = 0;

    char queryBuf_[512] = {};
    bool focusSearch_ = true;
    std::vector<RecordId> results_;
    double searchMs_ = 0;

    int selectedRow_ = -1;  // index into results_
    bool scrollToSelected_ = false;

    std::string statusMessage_;  // transient, e.g. "Copied path"
    std::chrono::steady_clock::time_point statusTime_;
};

}  // namespace fe
