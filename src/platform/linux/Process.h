#pragma once

#include <string>
#include <vector>

// Running other programs, the POSIX way: fork() makes a copy of our process,
// exec*() replaces that copy with another program. No shell is involved, so
// file names with spaces or quotes need no escaping.
namespace fe::posix {

// Start a program and forget about it (used for xdg-open). Returns false if it
// could not be started. The program keeps running after FindEverything exits.
bool spawnDetached(const std::vector<std::string>& argv);

// Run a program, wait for it, collect its stdout. Returns the exit code
// (-1 if it could not be run). Blocks, so only use it for quick commands.
int runCapture(const std::vector<std::string>& argv, std::string* output = nullptr);

}  // namespace fe::posix
