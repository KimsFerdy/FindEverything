#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct sd_bus;

namespace fe {

// tray icon over d-bus: StatusNotifierItem + a tiny com.canonical.dbusmenu.
// works on cinnamon (xapp-sn-watcher), kde, xfce. gnome needs the appindicator extension.
// runs its own thread, the ui just drains actions
class Tray {
public:
    enum class Action { Toggle, Show, Rescan, Quit };

    struct Icon {
        int width = 0, height = 0;
        std::vector<uint8_t> argb;  // ARGB32, network byte order (what the spec wants)
    };

    // wake gets called from the tray thread whenever there's a new action
    Tray(std::vector<Icon> icons, std::function<void()> wake);
    ~Tray();
    Tray(const Tray&) = delete;
    Tray& operator=(const Tray&) = delete;

    // registered with a watcher, i.e. the icon is actually on the panel
    bool available() const { return available_; }
    std::vector<Action> takeActions();

private:
    friend struct TrayDbus;

    void run();
    bool registerWithWatcher();
    void push(Action action);

    std::vector<Icon> icons_;
    std::function<void()> wake_;
    std::string busName_;
    sd_bus* bus_ = nullptr;  // only touched on the tray thread

    std::atomic<bool> available_{false};
    std::atomic<bool> stop_{false};
    std::atomic<bool> needRegister_{true};
    std::mutex mutex_;
    std::vector<Action> actions_;
    std::thread thread_;
};

}  // namespace fe
