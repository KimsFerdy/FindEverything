#pragma once

#include <string>
#include <vector>

#include "app/Settings.h"
#include "platform/Volumes.h"

namespace fe {

// edits a copy, nothing changes until OK
class PreferencesDialog {
public:
    enum class Result { None, Ok, Cancel };

    void open(const Settings& current);
    Result draw();  // every frame, returns Ok once on confirm
    const Settings& edited() const { return draft_; }

private:
    void drawGeneralTab();
    void drawIndexesTab();
    void drawAppearanceTab();

    void drawShortcutRecorder();

    bool requestOpen_ = false;
    bool recording_ = false;  // waiting for a key combo
    Settings draft_;
    std::vector<Volume> volumes_;
    char folderInput_[1024] = {};
};

}  // namespace fe
