#include "platform/Shell.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <spawn.h>
#include <sys/wait.h>

#include <vector>
extern char** environ;
#endif

namespace fe {

#if defined(_WIN32)

namespace {
std::wstring widen(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
}  // namespace

bool openPath(const std::string& path) {
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", widen(path).c_str(), nullptr, nullptr,
                                                   SW_SHOWNORMAL)) > 32;
}

bool revealInFileManager(const std::string& path) {
    std::wstring args = L"/select,\"" + widen(path) + L"\"";
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr,
                                                   SW_SHOWNORMAL)) > 32;
}

#elif defined(__APPLE__)

namespace {
bool runOpen(std::vector<std::string> args) {
    std::vector<char*> argv;
    for (std::string& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);
    pid_t pid;
    if (posix_spawnp(&pid, "open", nullptr, nullptr, argv.data(), environ) != 0) return false;
    int status = 0;
    waitpid(pid, &status, 0);  // `open` returns immediately after handing off to Finder
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
}  // namespace

bool openPath(const std::string& path) { return runOpen({"open", path}); }
bool revealInFileManager(const std::string& path) { return runOpen({"open", "-R", path}); }

#else

bool openPath(const std::string&) { return false; }
bool revealInFileManager(const std::string&) { return false; }

#endif

}  // namespace fe
