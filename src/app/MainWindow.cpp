#include "app/MainWindow.h"

#include "core/Query.h"
#include "core/Search.h"

#include "imgui.h"

#include <cstdio>
#include <ctime>
#include <string>

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
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
    return buf;
}

double msSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

MainWindow::~MainWindow() {
    progress_.cancel = true;
    if (scanThread_.joinable()) scanThread_.join();
}

void MainWindow::startIndexing() {
    scanStart_ = std::chrono::steady_clock::now();
    scanThread_ = std::thread([this] {
        auto index = std::make_unique<Index>();
        ScanOptions opts;
        opts.roots = defaultRoots();
        opts.excludes = defaultExcludes();
        scanFilesystem(*index, opts, &progress_);

        std::lock_guard lock(pendingMutex_);
        pending_ = std::move(index);
    });
}

void MainWindow::runSearch() {
    if (!index_) return;
    auto t0 = std::chrono::steady_clock::now();
    results_ = search(*index_, parseQuery(queryBuf_));
    searchMs_ = msSince(t0);
}

void MainWindow::draw() {
    {
        std::lock_guard lock(pendingMutex_);
        if (pending_) {
            index_ = std::move(pending_);
            scanSeconds_ = msSince(scanStart_) / 1000.0;
            runSearch();
        }
    }

    // One borderless window filling the whole OS window.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("FindEverything", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    if (ImGui::InputTextWithHint("##query", "Search (e.g.  report ext:pdf;docx !old)", queryBuf_, sizeof queryBuf_)) {
        runSearch();  // search on every keystroke, like Everything
    }

    drawResultsTable();
    drawStatusBar();
    ImGui::End();
}

void MainWindow::drawResultsTable() {
    const float statusBarHeight = ImGui::GetFrameHeightWithSpacing();
    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                  ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("results", 4, flags, ImVec2(0, -statusBarHeight))) return;

    ImGui::TableSetupScrollFreeze(0, 1);  // keep header row visible
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Date Modified", ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableHeadersRow();

    if (index_) {
        // The clipper only gives us the rows currently on screen, so a list of
        // 5 million results costs the same to draw as a list of 30.
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(results_.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const RecordId id = results_[row];
                const FileRecord& r = index_->record(id);
                const bool isDir = r.flags & kIsDir;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                std::string_view name = index_->name(id);
                if (isDir) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(240, 200, 90, 255));
                ImGui::TextUnformatted(name.data(), name.data() + name.size());
                if (isDir) ImGui::PopStyleColor();

                ImGui::TableNextColumn();
                std::string parentPath = r.parent == kNoParent ? "" : index_->fullPath(r.parent);
                ImGui::TextUnformatted(parentPath.c_str());

                ImGui::TableNextColumn();
                if (!isDir) ImGui::TextUnformatted(formatSize(r.size).c_str());

                ImGui::TableNextColumn();
                if (r.mtime) ImGui::TextUnformatted(formatTime(r.mtime).c_str());
            }
        }
    }
    ImGui::EndTable();
}

void MainWindow::drawStatusBar() {
    if (!index_) {
        ImGui::Text("Indexing... %llu objects", static_cast<unsigned long long>(progress_.entries.load()));
    } else {
        ImGui::Text("%zu objects  |  %zu results  |  search %.1f ms  |  indexed in %.1f s", index_->size(),
                    results_.size(), searchMs_, scanSeconds_);
    }
}

}  // namespace fe
