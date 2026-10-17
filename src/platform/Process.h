#pragma once

#include <string>
#include <vector>

// fork + exec, no shell involved so no quoting headaches
namespace fe::posix {

// fire and forget (xdg-open). keeps running after we exit
bool spawnDetached(const std::vector<std::string>& argv);

// run, wait, grab stdout. returns exit code or -1. blocks, only for quick commands
int runCapture(const std::vector<std::string>& argv, std::string* output = nullptr);

}  // namespace fe::posix
