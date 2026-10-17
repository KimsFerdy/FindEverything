#include "platform/Volumes.h"

#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fe {

std::vector<Volume> listVolumes() {
    std::vector<Volume> volumes;
#if defined(_WIN32)
    const DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(mask & (1u << i))) continue;
        const std::string root = std::string(1, static_cast<char>('A' + i)) + ":\\";
        const UINT type = GetDriveTypeA(root.c_str());
        if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE) continue;  // skip CD, network, RAM disks

        char label[MAX_PATH + 1] = {}, fs[MAX_PATH + 1] = {};
        GetVolumeInformationA(root.c_str(), label, sizeof label, nullptr, nullptr, nullptr, fs, sizeof fs);
        volumes.push_back({root, label, fs, type == DRIVE_REMOVABLE});
    }
#else
    volumes.push_back({"/", "", "", false});
#if defined(__APPLE__)
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("/Volumes", ec)) {
        if (entry.is_symlink(ec)) continue;  // "Macintosh HD" -> "/"
        volumes.push_back({entry.path().string(), entry.path().filename().string(), "", true});
    }
#endif
#endif
    return volumes;
}

}  // namespace fe
