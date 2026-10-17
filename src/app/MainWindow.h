#pragma once

#include "core/Index.h"
#include "platform/Scanner.h"

#include <chrono>
#include <memory>
#include <mutex>
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
    void drawResultsTable();
    void drawStatusBar();

    std::unique_ptr<Index> index_;  // owned by the UI thread
    std::unique_ptr<Index> pending_;
    std::mutex pendingMutex_;
    std::thread scanThread_;
    ScanProgress progress_;
    std::chrono::steady_clock::time_point scanStart_;
    double scanSeconds_ = 0;

    char queryBuf_[512] = {};
    std::vector<RecordId> results_;
    double searchMs_ = 0;
};

}  // namespace fe
