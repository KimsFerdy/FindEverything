#include "app/UpdateChecker.h"

#include <cstdio>
#include <tuple>

#include "platform/Process.h"

#ifndef FE_VERSION
#define FE_VERSION "dev"
#endif

namespace fe {

namespace {

constexpr const char* kRepo = "KimsFerdy/FindEverything";

struct Version {
    int major = 0, minor = 0, patch = 0;
    bool prerelease = false;  // "-beta" etc

    auto key() const { return std::tie(major, minor, patch); }
};

// "v0.2.0-beta" -> {0, 2, 0, prerelease}
Version parseVersion(const std::string& s) {
    Version v;
    const char* p = s.c_str();
    if (*p == 'v') ++p;
    std::sscanf(p, "%d.%d.%d", &v.major, &v.minor, &v.patch);
    v.prerelease = s.find('-') != std::string::npos;
    return v;
}

// good enough for one string field of the github response, no json lib for this
std::string jsonString(const std::string& json, const std::string& key) {
    const size_t k = json.find("\"" + key + "\"");
    if (k == std::string::npos) return {};
    const size_t open = json.find('"', json.find(':', k) + 1);
    const size_t close = json.find('"', open + 1);
    if (open == std::string::npos || close == std::string::npos) return {};
    return json.substr(open + 1, close - open - 1);
}

}  // namespace

UpdateChecker::~UpdateChecker() {
    if (thread_.joinable()) thread_.join();  // curl has a 10s timeout, so this can't hang forever
}

void UpdateChecker::start() {
    if (running_) return;
    if (thread_.joinable()) thread_.join();
    running_ = true;
    thread_ = std::thread([this] { run(); });
}

bool UpdateChecker::takeResult() { return finished_.exchange(false); }

UpdateChecker::Result UpdateChecker::result() const {
    std::lock_guard lock(mutex_);
    return result_;
}

void UpdateChecker::run() {
    Result r;
    r.current = FE_VERSION;

    // /releases/latest skips pre-releases, which is what we want to compare against
    std::string body;
    const int rc = posix::runCapture({"curl", "-fsSL", "--max-time", "10", "-H", "Accept: application/vnd.github+json",
                                      "-H", "User-Agent: FindEverything/" + r.current,
                                      std::string("https://api.github.com/repos/") + kRepo + "/releases/latest"},
                                     &body);
    const std::string tag = rc == 0 ? jsonString(body, "tag_name") : "";
    if (!tag.empty()) {
        const Version latest = parseVersion(tag), current = parseVersion(r.current);
        r.latest = tag[0] == 'v' ? tag.substr(1) : tag;
        r.url = std::string("https://github.com/") + kRepo + "/releases/tag/" + tag;
        if (latest.key() > current.key() || (latest.key() == current.key() && current.prerelease))
            r.status = Result::Status::UpdateAvailable;
        else if (latest.key() < current.key())
            r.status = Result::Status::AheadOfRelease;
        else
            r.status = Result::Status::UpToDate;
    } else {
        r.url = std::string("https://github.com/") + kRepo + "/releases";
    }

    {
        std::lock_guard lock(mutex_);
        result_ = std::move(r);
    }
    running_ = false;
    finished_ = true;
}

}  // namespace fe
