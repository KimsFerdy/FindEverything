#include "app/PreferencesDialog.h"

#include "app/Theme.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace fe {

namespace {

constexpr const char* kTitle = "Preferences";

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

    // Tabs fill the window except for the button row at the bottom.
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
    if (ImGui::Button("Cancel", button) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) result = Result::Cancel;
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
    hint("Starts minimized, so the index is ready the first time you need it.");
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Checkbox("Rescan disks on startup", &draft_.rescanOnStartup);
    ImGui::Indent();
    hint("Off: start with the saved index and only rescan when you press F5. "
         "Faster and quieter, but new files won't show up until you do.");
    ImGui::Unindent();
}

void PreferencesDialog::drawIndexesTab() {
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
                if (included) list.erase(std::remove(list.begin(), list.end(), v.path), list.end());
                else list.push_back(v.path);
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
        while (folder.size() > 1 && (folder.back() == '/' || folder.back() == '\\')) folder.pop_back();
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
