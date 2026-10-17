#pragma once

#include "app/Settings.h"
#include "platform/Volumes.h"

#include <string>
#include <vector>

namespace fe {

// Edit -> Preferences. Edits a copy of the settings; nothing changes until OK.
class PreferencesDialog {
public:
    enum class Result { None, Ok, Cancel };

    void open(const Settings& current);
    Result draw();  // call every frame; returns Ok once when the user confirms
    const Settings& edited() const { return draft_; }

private:
    void drawGeneralTab();
    void drawIndexesTab();
    void drawAppearanceTab();

    bool requestOpen_ = false;
    Settings draft_;
    std::vector<Volume> volumes_;
    char folderInput_[1024] = {};
};

}  // namespace fe
