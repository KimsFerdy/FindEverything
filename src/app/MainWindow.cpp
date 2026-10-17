#include "app/MainWindow.h"

#include "app/Theme.h"
#include "core/IndexFile.h"
#include "core/Search.h"
#include "platform/Paths.h"
#include "platform/Shell.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

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

std::string formatAge(int64_t unixSeconds) {
    const int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    const int64_t s = std::max<int64_t>(0, now - unixSeconds);
    char buf[32];
    if (s < 60) std::snprintf(buf, sizeof buf, "just now");
    else if (s < 3600) std::snprintf(buf, sizeof buf, "%lld min ago", static_cast<long long>(s / 60));
    else if (s < 86400) std::snprintf(buf, sizeof buf, "%lld h ago", static_cast<long long>(s / 3600));
    else std::snprintf(buf, sizeof buf, "%lld days ago", static_cast<long long>(s / 86400));
    return buf;
}

std::string joinLines(const std::vector<std::string>& items) {
    std::string out;
    for (const std::string& s : items) out += s + '\n';
    return out;
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

void MainWindow::start() {
    cachePath_ = indexCachePath();
    scanSource_ = joinLines(defaultRoots());

    // Show yesterday's index right away (a sequential read of a few dozen MB),
    // then refresh it. Without live updates (M3) the cache can be out of date,
    // so the rescan is what makes results trustworthy again.
    if (!cachePath_.empty()) {
        auto index = std::make_unique<Index>();
        IndexFileInfo info;
        if (loadIndex(*index, cachePath_, &info) && info.source == scanSource_) {
            cachedIndexTime_ = info.createdAt;
            adoptIndex(std::move(index));
        }
    }
    startIndexing();
}

void MainWindow::startIndexing() {
    if (scanning_) return;
    if (scanThread_.joinable()) scanThread_.join();  // previous scan, already finished

    scanning_ = true;
    progress_.entries = 0;
    progress_.errors = 0;
    progress_.cancel = false;
    scanStart_ = std::chrono::steady_clock::now();

    // The old index stays searchable while the new one is built.
    scanThread_ = std::thread([this, cachePath = cachePath_, source = scanSource_] {
        auto index = std::make_unique<Index>();
        ScanOptions opts;
        opts.roots = defaultRoots();
        opts.excludes = defaultExcludes();
        if (!scanFilesystem(*index, opts, &progress_)) return;  // cancelled: app is closing

        // Saved from this thread so the UI never waits for the disk.
        if (!cachePath.empty()) saveIndex(*index, cachePath, source);

        std::lock_guard lock(pendingMutex_);
        pending_ = std::move(index);
    });
}

void MainWindow::adoptIndex(std::unique_ptr<Index> index) {
    // Keep the same row selected across the swap (record ids change between scans).
    std::string selectedPath = (index_ && selectedRow_ >= 0) ? index_->fullPath(results_[selectedRow_]) : "";

    index_ = std::move(index);
    paths_.build(*index_);
    runSearch();

    if (selectedPath.empty()) return;
    std::string_view selectedName = std::string_view(selectedPath).substr(selectedPath.find_last_of("/\\") + 1);
    for (size_t i = 0; i < results_.size(); ++i) {
        if (index_->name(results_[i]) == selectedName && index_->fullPath(results_[i]) == selectedPath) {
            selectedRow_ = static_cast<int>(i);
            break;
        }
    }
}

bool MainWindow::consumeFontZoomChange() {
    const bool changed = fontZoomChanged_;
    fontZoomChanged_ = false;
    return changed;
}

void MainWindow::setZoom(float zoom) {
    zoom = std::clamp(zoom, 0.7f, 2.0f);
    if (zoom == fontZoom_) return;
    fontZoom_ = zoom;
    fontZoomChanged_ = true;
}

void MainWindow::runSearch() {
    if (!index_) return;
    auto t0 = std::chrono::steady_clock::now();
    results_ = search(*index_, parseQuery(queryBuf_, searchOptions_), 0, &paths_);
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

void MainWindow::copySelectedName() {
    if (selectedRow_ < 0) return;
    ImGui::SetClipboardText(std::string(index_->name(results_[selectedRow_])).c_str());
    setStatus("Copied name");
}

void MainWindow::draw() {
    {
        std::lock_guard lock(pendingMutex_);
        if (pending_) {
            adoptIndex(std::move(pending_));
            scanning_ = false;
            cachedIndexTime_ = 0;
            scanSeconds_ = msSince(scanStart_) / 1000.0;
        }
    }

    // One borderless window filling the whole OS window.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("FindEverything", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);

    drawMenuBar();
    handleKeyboard();

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (focusSearch_) {
        ImGui::SetKeyboardFocusHere();
        focusSearch_ = false;
    }
    if (ImGui::InputTextWithHint("##query", "Search  (e.g.  report ext:pdf;docx !old)", queryBuf_, sizeof queryBuf_)) {
        runSearch();  // search on every keystroke, like Everything
    }

    drawResultsTable();
    if (showStatusBar_) drawStatusBar();
    drawHelpPopups();
    ImGui::End();
}

void MainWindow::drawMenuBar() {
    if (!ImGui::BeginMenuBar()) return;
    const bool hasSelection = index_ && selectedRow_ >= 0;

    // Shortcut labels are only displayed here; the keys are handled in handleKeyboard().
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open", "Enter", false, hasSelection)) openSelected();
        if (ImGui::MenuItem("Open Containing Folder", "Ctrl+Enter", false, hasSelection)) revealSelected();
        ImGui::Separator();
        if (ImGui::MenuItem("Rescan", "F5", false, !scanning_)) startIndexing();
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Ctrl+Q")) quit_ = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Copy Full Path", "Ctrl+Shift+C", false, hasSelection)) copySelectedPath();
        if (ImGui::MenuItem("Copy Name", nullptr, false, hasSelection)) copySelectedName();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        columnsChanged_ |= ImGui::MenuItem("Path Column", nullptr, &columnVisible_[kPath]);
        columnsChanged_ |= ImGui::MenuItem("Size Column", nullptr, &columnVisible_[kSize]);
        columnsChanged_ |= ImGui::MenuItem("Date Modified Column", nullptr, &columnVisible_[kModified]);
        ImGui::MenuItem("Status Bar", nullptr, &showStatusBar_);
        ImGui::Separator();
        if (ImGui::MenuItem("Zoom In", "Ctrl+=")) setZoom(fontZoom_ + 0.1f);
        if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) setZoom(fontZoom_ - 0.1f);
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
        if (ImGui::MenuItem("About FindEverything")) openAbout_ = true;
        ImGui::EndMenu();
    }

    // Right-aligned hint showing which search options are active.
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

    const bool ctrl = ImGui::GetIO().KeyCtrl;
    const bool shift = ImGui::GetIO().KeyShift;
    auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, false); };

    if (ctrl && pressed(ImGuiKey_Q)) quit_ = true;
    if (pressed(ImGuiKey_F1)) openSyntaxHelp_ = true;
    if (pressed(ImGuiKey_F5)) startIndexing();
    if ((ctrl && pressed(ImGuiKey_F)) || pressed(ImGuiKey_Escape)) focusSearch_ = true;

    if (ctrl && (pressed(ImGuiKey_Equal) || pressed(ImGuiKey_KeypadAdd))) setZoom(fontZoom_ + 0.1f);
    if (ctrl && (pressed(ImGuiKey_Minus) || pressed(ImGuiKey_KeypadSubtract))) setZoom(fontZoom_ - 0.1f);
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
        selectedRow_ = std::clamp(selectedRow_ < 0 ? 0 : selectedRow_ + delta, 0, count - 1);
        scrollToSelected_ = true;
    };
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) moveSelection(1);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) moveSelection(-1);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) moveSelection(20);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) moveSelection(-20);

    if (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter)) {
        if (ctrl) revealSelected();  // Ctrl+Enter: open containing folder (as in Everything)
        else openSelected();
    }
    if (ctrl && shift && pressed(ImGuiKey_C)) copySelectedPath();
}

void MainWindow::drawResultsTable() {
    const float statusBarHeight = showStatusBar_ ? ImGui::GetFrameHeightWithSpacing() : 0.0f;
    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                  ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable;
    if (!ImGui::BeginTable("results", kColumnCount, flags, ImVec2(0, -statusBarHeight))) return;

    ImGui::TableSetupScrollFreeze(0, 1);  // keep header row visible
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide, 2.0f);
    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 6);
    ImGui::TableSetupColumn("Date Modified", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 9);

    // Columns can be hidden from the View menu *or* by right-clicking the header,
    // so sync both ways: menu change -> table, otherwise table -> menu checkmarks.
    for (int c = kPath; c < kColumnCount; ++c) {
        if (columnsChanged_) ImGui::TableSetColumnEnabled(c, columnVisible_[c]);
        else columnVisible_[c] = ImGui::TableGetColumnFlags(c) & ImGuiTableColumnFlags_IsEnabled;
    }
    columnsChanged_ = false;

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
                ImGui::TableSetColumnIndex(kName);
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

                // TableSetColumnIndex returns false for hidden columns: skip their work.
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
    if (ImGui::MenuItem("Open", "Enter")) openSelected();
    if (ImGui::MenuItem(isDir ? "Open Parent Folder" : "Open Containing Folder", "Ctrl+Enter")) revealSelected();
    ImGui::Separator();
    if (ImGui::MenuItem("Copy Full Path", "Ctrl+Shift+C")) copySelectedPath();
    if (ImGui::MenuItem("Copy Name")) copySelectedName();
}

void MainWindow::drawStatusBar() {
    ImGui::PushStyleColor(ImGuiCol_Text, themeColors().dimText);
    const bool showMessage = !statusMessage_.empty() && msSince(statusTime_) < 3000;
    const auto scanned = static_cast<unsigned long long>(progress_.entries.load());
    if (showMessage) {
        ImGui::TextUnformatted(statusMessage_.c_str());
    } else if (!index_) {
        ImGui::Text("Indexing...  %llu objects", scanned);
    } else if (scanning_) {
        std::string age = cachedIndexTime_ ? "index from " + formatAge(cachedIndexTime_) + ", updating" : "rescanning";
        ImGui::Text("%zu objects   ·   %zu results   ·   %s... %llu", index_->size(), results_.size(), age.c_str(),
                    scanned);
    } else {
        ImGui::Text("%zu objects   ·   %zu results   ·   search %.1f ms   ·   indexed in %.1f s", index_->size(),
                    results_.size(), searchMs_, scanSeconds_);
    }
    ImGui::PopStyleColor();
}

void MainWindow::drawHelpPopups() {
    // Opened here, not inside the menu, because popup IDs are scoped to the
    // window that opens them and the menu is its own window.
    if (openSyntaxHelp_) ImGui::OpenPopup("Search Syntax");
    if (openAbout_) ImGui::OpenPopup("About FindEverything");
    openSyntaxHelp_ = openAbout_ = false;

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Search Syntax", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static const char* kRows[][2] = {
            {"foo bar", "name contains foo AND bar"},
            {"\"foo bar\"", "exact phrase"},
            {"!tmp", "name does NOT contain tmp"},
            {"ext:jpg;png", "extension is jpg or png"},
            {"file:  folder:", "only files / only folders"},
            {"src/main", "a term with / matches the full path"},
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
        ImGui::TextColored(themeColors().dimText, "Search menu: Match Case (Ctrl+I), Whole Word (Ctrl+B), Path (Ctrl+U)");
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(ImGui::GetFontSize() * 6, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("About FindEverything", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("FindEverything %s", FE_VERSION);
        ImGui::TextColored(themeColors().dimText, "Instant file search, inspired by Everything by voidtools.");
        ImGui::TextColored(themeColors().dimText, "Built with Dear ImGui %s and GLFW.", IMGUI_VERSION);
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(ImGui::GetFontSize() * 6, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

}  // namespace fe
