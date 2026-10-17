#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace fe {

// Help -> Check for Updates. asks the github api for the latest release, only when clicked
class UpdateChecker {
public:
    struct Result {
        enum class Status { UpToDate, UpdateAvailable, AheadOfRelease, Failed } status = Status::Failed;
        std::string current;  // our version
        std::string latest;   // "0.1.0"
        std::string url;      // release page
    };

    ~UpdateChecker();

    void start();  // background, curl
    bool running() const { return running_; }
    bool takeResult();  // true once when a check finished
    Result result() const;

private:
    void run();

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> finished_{false};
    mutable std::mutex mutex_;
    Result result_;
};

}  // namespace fe
