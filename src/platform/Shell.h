#pragma once

#include <string>
#include <vector>

namespace fe {

// default app for the file (xdg-open)
bool openPath(const std::string& path);

// open the parent folder in the file manager with the item selected, if it supports that
bool revealInFileManager(const std::string& path);
bool revealInFileManager(const std::vector<std::string>& paths);  // one call, file manager selects them all

}  // namespace fe
