#pragma once

#include <string>

namespace fe {

// default app for the file (xdg-open)
bool openPath(const std::string& path);

// open the parent folder in the file manager with the item selected, if it supports that
bool revealInFileManager(const std::string& path);

}  // namespace fe
