#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct sd_bus;

namespace fe {

// single instance + remote control over the session bus.
// the first instance owns io.github.KimsFerdy.FindEverything, later launches (launcher click,
// global shortcut, --search) just send it a message and exit
class AppBus {
public:
    struct Request {
        enum class Kind { Show, Toggle } kind = Kind::Show;
        std::string search;  // "" = keep the current query
    };

    // primary: owns the name and serves requests on its own thread.
    // otherwise the request goes to the running instance and primary() is false
    AppBus(const Request& launch, std::function<void()> wake);
    ~AppBus();
    AppBus(const AppBus&) = delete;
    AppBus& operator=(const AppBus&) = delete;

    bool primary() const { return primary_; }
    std::vector<Request> takeRequests();

private:
    friend struct AppBusDbus;
    void run();
    void push(Request request);

    std::function<void()> wake_;
    sd_bus* bus_ = nullptr;
    bool primary_ = true;
    std::atomic<bool> stop_{false};
    std::mutex mutex_;
    std::vector<Request> requests_;
    std::thread thread_;
};

}  // namespace fe
