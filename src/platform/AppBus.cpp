#include "platform/AppBus.h"

#include <systemd/sd-bus.h>

#include <cerrno>
#include <string_view>
#include <utility>

namespace fe {

namespace {

constexpr const char* kName = "io.github.KimsFerdy.FindEverything";
constexpr const char* kPath = "/io/github/KimsFerdy/FindEverything";
constexpr const char* kIface = "io.github.KimsFerdy.FindEverything";

const char* kindName(AppBus::Request::Kind k) { return k == AppBus::Request::Kind::Toggle ? "toggle" : "show"; }

}  // namespace

struct AppBusDbus {
    // Activate(kind, search). try it: busctl --user call io.github.KimsFerdy.FindEverything
    //   /io/github/KimsFerdy/FindEverything io.github.KimsFerdy.FindEverything Activate ss show "ext:pdf"
    static int onActivate(sd_bus_message* m, void* userdata, sd_bus_error*) {
        const char *kind = nullptr, *search = nullptr;
        int r = sd_bus_message_read(m, "ss", &kind, &search);
        if (r < 0) return r;
        AppBus::Request req;
        req.kind = std::string_view(kind) == "toggle" ? AppBus::Request::Kind::Toggle : AppBus::Request::Kind::Show;
        req.search = search;
        static_cast<AppBus*>(userdata)->push(std::move(req));
        return sd_bus_reply_method_return(m, "");
    }
};

namespace {

const sd_bus_vtable kVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Activate", "ss", "", AppBusDbus::onActivate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END,
};

}  // namespace

AppBus::AppBus(const Request& launch, std::function<void()> wake) : wake_(std::move(wake)) {
    if (sd_bus_open_user(&bus_) < 0) {
        bus_ = nullptr;  // no session bus (ssh, tty...), just run as a normal standalone app
        return;
    }
    const int r = sd_bus_request_name(bus_, kName, 0);
    if (r == -EEXIST) {
        // someone's already running: hand them the request and get out
        primary_ = false;
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_call_method(bus_, kName, kPath, kIface, "Activate", &error, nullptr, "ss", kindName(launch.kind),
                           launch.search.c_str());
        sd_bus_error_free(&error);
        bus_ = sd_bus_flush_close_unref(bus_);
        return;
    }
    if (r < 0 || sd_bus_add_object_vtable(bus_, nullptr, kPath, kIface, kVtable, this) < 0) {
        bus_ = sd_bus_flush_close_unref(bus_);
        return;
    }
    thread_ = std::thread([this] { run(); });  // the bus only gets touched on that thread from here on
}

AppBus::~AppBus() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
    if (bus_) sd_bus_flush_close_unref(bus_);
}

std::vector<AppBus::Request> AppBus::takeRequests() {
    std::lock_guard lock(mutex_);
    return std::exchange(requests_, {});
}

void AppBus::push(Request request) {
    {
        std::lock_guard lock(mutex_);
        requests_.push_back(std::move(request));
    }
    if (wake_) wake_();
}

void AppBus::run() {
    while (!stop_) {
        const int r = sd_bus_process(bus_, nullptr);
        if (r < 0) break;
        if (r > 0) continue;
        sd_bus_wait(bus_, 250'000);
    }
}

}  // namespace fe
