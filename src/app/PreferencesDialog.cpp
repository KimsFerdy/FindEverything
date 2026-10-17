#include "app/PreferencesDialog.h"

#include <algorithm>
#include <cmath>

#include "app/Theme.h"
#include "imgui.h"
#include "platform/GlobalShortcut.h"

namespace fe {

namespace {

constexpr const char* kTitle = "Preferences";

// imgui key -> gtk accelerator key name
struct KeyName {
    ImGuiKey key;
    const char* name;
};
constexpr KeyName kKeyNames[] = {
    {ImGuiKey_Space, "space"},
    {ImGuiKey_Comma, "comma"},
    {ImGuiKey_Period, "period"},
    {ImGuiKey_Slash, "slash"},
    {ImGuiKey_Semicolon, "semicolon"},
    {ImGuiKey_Apostrophe, "apostrophe"},
    {ImGuiKey_Minus, "minus"},
    {ImGuiKey_Equal, "equal"},
    {ImGuiKey_GraveAccent, "grave"},
    {ImGuiKey_Backslash, "backslash"},
    {ImGuiKey_LeftBracket, "bracketleft"},
    {ImGuiKey_RightBracket, "bracketright"},
    {ImGuiKey_Insert, "Insert"},
    {ImGuiKey_Home, "Home"},
    {ImGuiKey_End, "End"},
    {ImGuiKey_PageUp, "Page_Up"},
    {ImGuiKey_PageDown, "Page_Down"},
    {ImGuiKey_Pause, "Pause"},
};

// the key pressed this frame as a gtk accelerator ("<Super>f"), "" if none / no modifier
std::string pressedAccel() {
    const ImGuiIO& io = ImGui::GetIO();
    std::string key;
    for (int k = ImGuiKey_A; k <= ImGuiKey_Z && key.empty(); ++k) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(k), false)) key = std::string(1, char('a' + (k - ImGuiKey_A)));
    }
    for (int k = ImGuiKey_0; k <= ImGuiKey_9 && key.empty(); ++k) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(k), false)) key = std::string(1, char('0' + (k - ImGuiKey_0)));
    }
    bool functionKey = false;
    for (int k = ImGuiKey_F1; k <= ImGuiKey_F12 && key.empty(); ++k) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(k), false)) {
            key = "F" + std::to_string(k - ImGuiKey_F1 + 1);
            functionKey = true;
        }
    }
    for (const KeyName& kn : kKeyNames) {
        if (key.empty() && ImGui::IsKeyPressed(kn.key, false)) key = kn.name;
    }
    const bool anyMod = io.KeyCtrl || io.KeyAlt || io.KeySuper || io.KeyShift;
    if (key.empty() || (!anyMod && !functionKey)) return {};  // plain letters would eat normal typing
    std::string accel;
    if (io.KeySuper) accel += "<Super>";
    if (io.KeyCtrl) accel += "<Primary>";
    if (io.KeyAlt) accel += "<Alt>";
    if (io.KeyShift) accel += "<Shift>";
    return accel + key;
}

bool contains(const std::vector<std::string>& list, const std::string& value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

void hint(const char* text) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(themeColors().dimText, "%s", text);
    ImGui::PopTextWrapPos();
}

}  // namespace

void PreferencesDialog::open(const Settings& current) {
    draft_ = current;
    recording_ = false;
    volumes_ = listVolumes();
    folderInput_[0] = '\0';
    requestOpen_ = true;
}

PreferencesDialog::Result PreferencesDialog::draw() {
    if (requestOpen_) {
        ImGui::OpenPopup(kTitle);
        requestOpen_ = false;
    }

    const float em = ImGui::GetFontSize();
    ImGui::SetNextWindowSize(ImVec2(em * 38, em * 28), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_NoSavedSettings)) return Result::None;

    // tabs take everything except the button row
    const float buttonRow = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("tabs", ImVec2(0, -buttonRow));
    if (ImGui::BeginTabBar("prefs_tabs")) {
        if (ImGui::BeginTabItem("General")) {
            drawGeneralTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Indexes")) {
            drawIndexesTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Appearance")) {
            drawAppearanceTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    Result result = Result::None;
    ImGui::Separator();
    const ImVec2 button(em * 6, 0);
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - button.x * 2 - ImGui::GetStyle().ItemSpacing.x -
                         ImGui::GetStyle().WindowPadding.x);
    if (ImGui::Button("Cancel", button) || (!recording_ && ImGui::IsKeyPressed(ImGuiKey_Escape, false)))
        result = Result::Cancel;
    ImGui::SameLine();
    if (ImGui::Button("OK", button)) result = Result::Ok;
    if (result != Result::None) ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
    return result;
}

void PreferencesDialog::drawGeneralTab() {
    ImGui::Spacing();
    ImGui::Checkbox("Start FindEverything when I log in", &draft_.launchAtLogin);
    ImGui::Indent();
    ImGui::BeginDisabled(!draft_.launchAtLogin || !draft_.trayIcon);
    ImGui::Checkbox("Start in the tray instead of opening the window", &draft_.startInTray);
    ImGui::EndDisabled();
    hint("The index is ready the first time you need it. Without the tray it starts minimized.");
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Checkbox("Show icon in the system tray", &draft_.trayIcon);
    ImGui::Indent();
    ImGui::BeginDisabled(!draft_.trayIcon);
    ImGui::Checkbox("Closing the window keeps FindEverything running in the tray", &draft_.closeToTray);
    ImGui::EndDisabled();
    hint("Left click the tray icon to show / hide the window, right click for the menu.");
    ImGui::Unindent();

    ImGui::Spacing();
    drawShortcutRecorder();

    ImGui::Spacing();
    ImGui::Checkbox("Rescan disks on startup", &draft_.rescanOnStartup);
    ImGui::Indent();
    hint(
        "Off: start with the saved index and only rescan when you press F5. "
        "Faster and quieter, but new files won't show up until you do.");
    ImGui::Unindent();
}

void PreferencesDialog::drawShortcutRecorder() {
    const ShortcutDesktop desktop = shortcutDesktop();
    ImGui::BeginDisabled(desktop == ShortcutDesktop::Unsupported);
    ImGui::Checkbox("Global shortcut", &draft_.globalShortcut);
    ImGui::SameLine();
    ImGui::BeginDisabled(!draft_.globalShortcut);
    const std::string label = recording_ ? std::string("Press a key combo...") : prettyAccel(draft_.shortcut);
    if (ImGui::Button((label + "###shortcut").c_str(), ImVec2(ImGui::GetFontSize() * 10, 0))) recording_ = true;
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (recording_) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            recording_ = false;
        } else if (std::string accel = pressedAccel(); !accel.empty()) {
            draft_.shortcut = accel;
            recording_ = false;
        }
    }

    ImGui::Indent();
    if (desktop == ShortcutDesktop::Unsupported) {
        hint(
            "Can't set it up automatically here. Add a shortcut in your desktop's keyboard settings "
            "that runs: FindEverything --toggle");
    } else {
        const std::string text = std::string("Shows / hides FindEverything from anywhere. Added to ") +
                                 shortcutDesktopName(desktop) + "'s keyboard shortcuts, you can change it there too.";
        hint(text.c_str());
    }
    ImGui::Unindent();
}

void PreferencesDialog::drawIndexesTab() {
    ImGui::Spacing();
    ImGui::Checkbox("Keep the index up to date while running", &draft_.liveUpdates);
    ImGui::Indent();
    hint(
        "New, renamed and deleted files show up within a second (inotify). "
        "Turn off to only update on rescan.");
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::TextUnformatted("Disks to index");
    hint("Each disk is scanned on its own. Changes start a rescan when you press OK.");

    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV;
    if (ImGui::BeginTable("volumes", 3, flags)) {
        ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Device", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 6);
        ImGui::TableHeadersRow();
        for (const Volume& v : volumes_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool included = !contains(draft_.excludedVolumes, v.path);
            if (ImGui::Checkbox(v.path.c_str(), &included)) {
                auto& list = draft_.excludedVolumes;
                if (included)
                    list.erase(std::remove(list.begin(), list.end(), v.path), list.end());
                else
                    list.push_back(v.path);
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(v.device.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s%s", v.fsType.c_str(), v.removable ? "  (removable)" : "");
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextUnformatted("Excluded folders");
    hint("These folders and everything inside them are left out of the index.");

    auto& folders = draft_.excludedFolders;
    for (size_t i = 0; i < folders.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton("Remove")) {
            folders.erase(folders.begin() + static_cast<std::ptrdiff_t>(i));
            ImGui::PopID();
            break;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(folders[i].c_str());
        ImGui::PopID();
    }

    const float addWidth = ImGui::GetFontSize() * 5;
    ImGui::SetNextItemWidth(-addWidth - ImGui::GetStyle().ItemSpacing.x);
    const bool enter = ImGui::InputTextWithHint("##folder", "/path/to/folder", folderInput_, sizeof folderInput_,
                                                ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button("Add", ImVec2(addWidth, 0)) || enter) && folderInput_[0]) {
        std::string folder = folderInput_;
        while (folder.size() > 1 && folder.back() == '/') folder.pop_back();
        if (!contains(folders, folder)) folders.push_back(folder);
        folderInput_[0] = '\0';
    }
}

void PreferencesDialog::drawAppearanceTab() {
    ImGui::Spacing();
    ImGui::TextUnformatted("Theme");
    int theme = static_cast<int>(draft_.theme);
    ImGui::RadioButton("Follow system", &theme, static_cast<int>(Settings::Theme::System));
    ImGui::SameLine();
    ImGui::RadioButton("Dark", &theme, static_cast<int>(Settings::Theme::Dark));
    ImGui::SameLine();
    ImGui::RadioButton("Light", &theme, static_cast<int>(Settings::Theme::Light));
    draft_.theme = static_cast<Settings::Theme>(theme);

    ImGui::Spacing();
    ImGui::Checkbox("Custom accent colour", &draft_.customAccent);
    ImGui::SameLine();
    ImGui::BeginDisabled(!draft_.customAccent);
    ImGui::ColorEdit3("##accent", draft_.accent, ImGuiColorEditFlags_NoInputs);
    ImGui::EndDisabled();
    hint("Off: use the desktop's accent colour.");

    ImGui::Spacing();
    ImGui::TextUnformatted("Font size");
    int percent = static_cast<int>(std::lround(draft_.fontZoom * 100));
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14);
    if (ImGui::SliderInt("##zoom", &percent, 70, 200, "%d%%")) draft_.fontZoom = static_cast<float>(percent) / 100.0f;
    hint("Also Ctrl+= / Ctrl+- / Ctrl+0 in the main window.");
}

}  // namespace fe
