#include "platform/linux/LinuxMounts.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <string_view>

namespace fe::linux_fs {

namespace {

// mountinfo escapes space, tab, newline and backslash as octal: "\040" == ' '.
std::string unescapeOctal(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 3 < s.size()) {
            const char a = s[i + 1], b = s[i + 2], c = s[i + 3];
            if (a >= '0' && a <= '7' && b >= '0' && b <= '7' && c >= '0' && c <= '7') {
                out += static_cast<char>((a - '0') * 64 + (b - '0') * 8 + (c - '0'));
                i += 3;
                continue;
            }
        }
        out += s[i];
    }
    return out;
}

}  // namespace

std::vector<MountInfo> readMounts() {
    // Line format (see `man 5 proc`, section /proc/pid/mountinfo):
    // 36 35 98:0 /mnt1 /mnt2 rw,noatime master:1 - ext3 /dev/root rw,errors=continue
    // (1)(2) (3)   (4)   (5)      (6)      (7)   (8) (9)   (10)        (11)
    // Field 7 is a variable-length list, terminated by the lone "-" in (8).
    std::vector<MountInfo> mounts;
    std::ifstream in("/proc/self/mountinfo");
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::vector<std::string> fields;
        for (std::string f; ss >> f;) fields.push_back(std::move(f));
        if (fields.size() < 7) continue;

        auto dash = std::find(fields.begin() + 6, fields.end(), "-");
        if (dash == fields.end() || std::distance(dash, fields.end()) < 3) continue;

        MountInfo m;
        m.mountPoint = unescapeOctal(fields[4]);
        m.fsType = *(dash + 1);
        m.source = *(dash + 2);
        mounts.push_back(std::move(m));
    }
    return mounts;
}

bool isPseudoFilesystem(const std::string& fsType) {
    static const std::array<std::string_view, 24> kPseudo = {
        "proc",     "sysfs",      "devtmpfs", "devpts",   "tmpfs",     "cgroup",
        "cgroup2",  "securityfs", "debugfs",  "tracefs",  "fusectl",   "configfs",
        "bpf",      "pstore",     "mqueue",   "hugetlbfs", "autofs",   "efivarfs",
        "binfmt_misc", "ramfs",   "nsfs",     "rpc_pipefs", "squashfs", "fuse.gvfsd-fuse",
    };
    return std::find(kPseudo.begin(), kPseudo.end(), fsType) != kPseudo.end();
}

}  // namespace fe::linux_fs
