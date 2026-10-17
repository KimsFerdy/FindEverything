#include "app/MainWindow.h"

#include "app/Theme.h"
#include "core/Query.h"
#include "core/Search.h"
#include "platform/Shell.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

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

// Tiny vector icons drawn with ImGui's draw list, so we need no icon font.
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
    selectedRow_ = -1;
}

void MainWindow::setStatus(std::string message) {
    statusMessage_ = std::move(message);
    statusTime_ = std::chrono::steady_clock::now();
}

void MainWindow::openSelected() {
    if (selectedRow_ < 0) return;
    std::string path = index_->fullPath(results_[selectedRow_]);
    if (!openPath(path)) setStatus("Could not open " + path);
}

void MainWindow::revealSelected() {
    if (selectedRow_ < 0) return;
    std::string path = index_->fullPath(results_[selectedRow_]);
    if (!revealInFileManager(path)) setStatus("Could not open folder for " + path);
}

void MainWindow::copySelectedPath() {
    if (selectedRow_ < 0) return;
    ImGui::SetClipboardText(index_->fullPath(results_[selectedRow_]).c_str());
    setStatus("Copied full path");
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

    if (index_) handleKeyboard();

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (focusSearch_) {
        ImGui::SetKeyboardFocusHere();
        focusSearch_ = false;
    }
    if (ImGui::InputTextWithHint("##query", "Search  (e.g.  report ext:pdf;docx !old)", queryBuf_, sizeof queryBuf_)) {
        runSearch();  // search on every keystroke, like Everything
    }

    drawResultsTable();
    drawStatusBar();
    ImGui::End();
}

void MainWindow::handleKeyboard() {
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) return;

    const bool ctrl = ImGui::GetIO().KeyCtrl;
    const bool shift = ImGui::GetIO().KeyShift;
    const int count = static_cast<int>(results_.size());

    auto moveSelection = [&](int delta) {
        if (count == 0) return;
        selectedRow_ = std::clamp(selectedRow_ < 0 ? 0 : selectedRow_ + delta, 0, count - 1);
        scrollToSelected_ = true;
    };
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) moveSelection(1);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) moveSelection(-1);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) moveSelection(20);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) moveSelection(-20);

    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
        if (ctrl) revealSelected();  // Ctrl+Enter: open containing folder (as in Everything)
        else openSelected();
    }
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_C, false)) copySelectedPath();
    if ((ctrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        focusSearch_ = true;
    }
}

void MainWindow::drawResultsTable() {
    const float statusBarHeight = ImGui::GetFrameHeightWithSpacing();
    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                  ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("results", 4, flags, ImVec2(0, -statusBarHeight))) return;

    ImGui::TableSetupScrollFreeze(0, 1);  // keep header row visible
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 6);
    ImGui::TableSetupColumn("Date Modified", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 9);
    ImGui::TableHeadersRow();

    if (index_) {
        const ThemeColors& theme = themeColors();
        const float lineHeight = ImGui::GetTextLineHeight();
        const float iconWidth = lineHeight + ImGui::GetStyle().ItemInnerSpacing.x;

        // The clipper only gives us the rows currently on screen, so a list of
        // 5 million results costs the same to draw as a list of 30.
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(results_.size()));
        if (scrollToSelected_ && selectedRow_ >= 0) clipper.IncludeItemByIndex(selectedRow_);
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const RecordId id = results_[row];
                const FileRecord& r = index_->record(id);
                const bool isDir = r.flags & kIsDir;
                const bool selected = row == selectedRow_;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(row);

                // An invisible full-row button handles clicks; we draw the content on top.
                const ImVec2 start = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##row", selected,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap |
                                          ImGuiSelectableFlags_AllowDoubleClick)) {
                    selectedRow_ = row;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) openSelected();
                }
                if (ImGui::BeginPopupContextItem("row_menu")) {
                    selectedRow_ = row;
                    drawRowContextMenu(id);
                    ImGui::EndPopup();
                }
                if (selected && scrollToSelected_) {
                    ImGui::SetScrollHereY(0.5f);
                    scrollToSelected_ = false;
                }

                if (selected) ImGui::PushStyleColor(ImGuiCol_Text, theme.selectedText);
                drawIcon(start, lineHeight, isDir, selected ? theme.selectedText : isDir ? theme.folderIcon : theme.fileIcon);
                ImGui::SetCursorScreenPos(ImVec2(start.x + iconWidth, start.y));
                std::string_view name = index_->name(id);
                ImGui::TextUnformatted(name.data(), name.data() + name.size());

                ImGui::TableNextColumn();
                std::string parentPath = r.parent == kNoParent ? "" : index_->fullPath(r.parent);
                ImGui::TextUnformatted(parentPath.c_str());

                ImGui::TableNextColumn();
                if (!isDir) ImGui::TextUnformatted(formatSize(r.size).c_str());

                ImGui::TableNextColumn();
                if (r.mtime) ImGui::TextUnformatted(formatTime(r.mtime).c_str());
                if (selected) ImGui::PopStyleColor();

                ImGui::PopID();
            }
        }
    }
    ImGui::EndTable();
}

void MainWindow::drawRowContextMenu(RecordId id) {
    const bool isDir = index_->record(id).flags & kIsDir;
    if (ImGui::MenuItem("Open", "Enter")) openSelected();
    if (ImGui::MenuItem(isDir ? "Open Parent Folder" : "Open Containing Folder", "Ctrl+Enter")) revealSelected();
    ImGui::Separator();
    if (ImGui::MenuItem("Copy Full Path", "Ctrl+Shift+C")) copySelectedPath();
    if (ImGui::MenuItem("Copy Name")) {
        ImGui::SetClipboardText(std::string(index_->name(id)).c_str());
        setStatus("Copied name");
    }
}

void MainWindow::drawStatusBar() {
    ImGui::PushStyleColor(ImGuiCol_Text, themeColors().dimText);
    const bool showMessage = !statusMessage_.empty() && msSince(statusTime_) < 3000;
    if (showMessage) {
        ImGui::TextUnformatted(statusMessage_.c_str());
    } else if (!index_) {
        ImGui::Text("Indexing...  %llu objects", static_cast<unsigned long long>(progress_.entries.load()));
    } else {
        ImGui::Text("%zu objects   ·   %zu results   ·   search %.1f ms   ·   indexed in %.1f s", index_->size(),
                    results_.size(), searchMs_, scanSeconds_);
    }
    ImGui::PopStyleColor();
}

}  // namespace fe
