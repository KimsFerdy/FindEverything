#pragma once

#include <string>

namespace fe {

// Open a file or folder with the user's default application.
bool openPath(const std::string& path);

// Open the containing folder in the file manager, with the item selected when
// the file manager supports it.
bool revealInFileManager(const std::string& path);

}  // namespace fe
