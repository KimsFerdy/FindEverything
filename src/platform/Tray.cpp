#include "platform/Tray.h"

#include <systemd/sd-bus.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <string_view>
#include <utility>

namespace fe {

namespace {

constexpr const char* kItemPath = "/StatusNotifierItem";
constexpr const char* kItemIface = "org.kde.StatusNotifierItem";
constexpr const char* kMenuPath = "/MenuBar";
constexpr const char* kMenuIface = "com.canonical.dbusmenu";
constexpr const char* kWatcher = "org.kde.StatusNotifierWatcher";
constexpr uint32_t kMenuRevision = 1;

struct MenuEntry {
    int32_t id;
    const char* label;
    Tray::Action action;
    bool separator;
};

constexpr MenuEntry kMenu[] = {
    {1, "Show FindEverything", Tray::Action::Show, false},
    {2, "Rescan", Tray::Action::Rescan, false},
    {3, "", Tray::Action::Show, true},
    {4, "Quit", Tray::Action::Quit, false},
};

const MenuEntry* findEntry(int32_t id) {
    for (const MenuEntry& e : kMenu) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

int appendItemProps(sd_bus_message* m, const MenuEntry& e) {
    int r = sd_bus_message_open_container(m, 'a', "{sv}");
    if (r < 0) return r;
    if (e.separator) {
        r = sd_bus_message_append(m, "{sv}", "type", "s", "separator");
    } else {
        r = sd_bus_message_append(m, "{sv}", "label", "s", e.label);
        if (r >= 0) r = sd_bus_message_append(m, "{sv}", "enabled", "b", 1);
    }
    if (r < 0) return r;
    return sd_bus_message_close_container(m);
}

// one menu item as (ia{sv}av), no children
int appendItem(sd_bus_message* m, const MenuEntry& e) {
    int r = sd_bus_message_open_container(m, 'r', "ia{sv}av");
    if (r >= 0) r = sd_bus_message_append(m, "i", e.id);
    if (r >= 0) r = appendItemProps(m, e);
    if (r >= 0) r = sd_bus_message_open_container(m, 'a', "v");
    if (r >= 0) r = sd_bus_message_close_container(m);
    if (r >= 0) r = sd_bus_message_close_container(m);
    return r;
}

}  // namespace

// sd-bus callbacks. friend of Tray so they can reach its guts
struct TrayDbus {
    static Tray* self(void* userdata) { return static_cast<Tray*>(userdata); }

    static int itemProperty(sd_bus*, const char*, const char*, const char* property, sd_bus_message* reply,
                            void* userdata, sd_bus_error*) {
        const std::string_view p = property;
        if (p == "Category") return sd_bus_message_append(reply, "s", "ApplicationStatus");
        if (p == "Id") return sd_bus_message_append(reply, "s", "findeverything");
        if (p == "Title") return sd_bus_message_append(reply, "s", "FindEverything");
        if (p == "Status") return sd_bus_message_append(reply, "s", "Active");
        if (p == "IconName") return sd_bus_message_append(reply, "s", "");
        if (p == "ItemIsMenu") return sd_bus_message_append(reply, "b", 0);
        if (p == "Menu") return sd_bus_message_append(reply, "o", kMenuPath);
        if (p == "WindowId") return sd_bus_message_append(reply, "i", 0);
        if (p == "IconPixmap") {
            int r = sd_bus_message_open_container(reply, 'a', "(iiay)");
            for (const Tray::Icon& icon : self(userdata)->icons_) {
                if (r >= 0) r = sd_bus_message_open_container(reply, 'r', "iiay");
                if (r >= 0) r = sd_bus_message_append(reply, "ii", icon.width, icon.height);
                if (r >= 0) r = sd_bus_message_append_array(reply, 'y', icon.argb.data(), icon.argb.size());
                if (r >= 0) r = sd_bus_message_close_container(reply);
            }
            return r < 0 ? r : sd_bus_message_close_container(reply);
        }
        if (p == "ToolTip") {  // (icon name, icon pixmaps, title, body)
            int r = sd_bus_message_open_container(reply, 'r', "sa(iiay)ss");
            if (r >= 0) r = sd_bus_message_append(reply, "s", "");
            if (r >= 0) r = sd_bus_message_open_container(reply, 'a', "(iiay)");
            if (r >= 0) r = sd_bus_message_close_container(reply);
            if (r >= 0) r = sd_bus_message_append(reply, "ss", "FindEverything", "Instant file search");
            return r < 0 ? r : sd_bus_message_close_container(reply);
        }
        return -EINVAL;
    }

    static int menuProperty(sd_bus*, const char*, const char*, const char* property, sd_bus_message* reply, void*,
                            sd_bus_error*) {
        const std::string_view p = property;
        if (p == "Version") return sd_bus_message_append(reply, "u", 3u);
        if (p == "TextDirection") return sd_bus_message_append(reply, "s", "ltr");
        if (p == "Status") return sd_bus_message_append(reply, "s", "normal");
        if (p == "IconThemePath") return sd_bus_message_append(reply, "as", 0);
        return -EINVAL;
    }

    static int onActivate(sd_bus_message* m, void* userdata, sd_bus_error*) {
        self(userdata)->push(Tray::Action::Toggle);
        return sd_bus_reply_method_return(m, "");
    }

    static int onContextMenu(sd_bus_message* m, void* userdata, sd_bus_error*) {
        // hosts that ignore our Menu call this on right click, best we can do is show the window
        self(userdata)->push(Tray::Action::Show);
        return sd_bus_reply_method_return(m, "");
    }

    static int onIgnored(sd_bus_message* m, void*, sd_bus_error*) { return sd_bus_reply_method_return(m, ""); }

    // root (id 0) + the flat list above, we don't do submenus
    static int onGetLayout(sd_bus_message* m, void*, sd_bus_error*) {
        sd_bus_message* reply = nullptr;
        int r = sd_bus_message_new_method_return(m, &reply);
        if (r < 0) return r;
        r = sd_bus_message_append(reply, "u", kMenuRevision);
        if (r >= 0) r = sd_bus_message_open_container(reply, 'r', "ia{sv}av");
        if (r >= 0) r = sd_bus_message_append(reply, "i", 0);
        if (r >= 0) r = sd_bus_message_open_container(reply, 'a', "{sv}");
        if (r >= 0) r = sd_bus_message_append(reply, "{sv}", "children-display", "s", "submenu");
        if (r >= 0) r = sd_bus_message_close_container(reply);
        if (r >= 0) r = sd_bus_message_open_container(reply, 'a', "v");
        for (const MenuEntry& e : kMenu) {
            if (r >= 0) r = sd_bus_message_open_container(reply, 'v', "(ia{sv}av)");
            if (r >= 0) r = appendItem(reply, e);
            if (r >= 0) r = sd_bus_message_close_container(reply);
        }
        if (r >= 0) r = sd_bus_message_close_container(reply);
        if (r >= 0) r = sd_bus_message_close_container(reply);
        if (r >= 0) r = sd_bus_send(nullptr, reply, nullptr);
        sd_bus_message_unref(reply);
        return r;
    }

    static int onGetGroupProperties(sd_bus_message* m, void*, sd_bus_error*) {
        std::vector<int32_t> ids;
        int r = sd_bus_message_enter_container(m, 'a', "i");
        int32_t id;
        while (r >= 0 && (r = sd_bus_message_read(m, "i", &id)) > 0) ids.push_back(id);
        if (r < 0) return r;
        sd_bus_message_exit_container(m);
        if (ids.empty()) {
            for (const MenuEntry& e : kMenu) ids.push_back(e.id);
        }

        sd_bus_message* reply = nullptr;
        r = sd_bus_message_new_method_return(m, &reply);
        if (r < 0) return r;
        r = sd_bus_message_open_container(reply, 'a', "(ia{sv})");
        for (int32_t want : ids) {
            const MenuEntry* e = findEntry(want);
            if (!e) continue;
            if (r >= 0) r = sd_bus_message_open_container(reply, 'r', "ia{sv}");
            if (r >= 0) r = sd_bus_message_append(reply, "i", e->id);
            if (r >= 0) r = appendItemProps(reply, *e);
            if (r >= 0) r = sd_bus_message_close_container(reply);
        }
        if (r >= 0) r = sd_bus_message_close_container(reply);
        if (r >= 0) r = sd_bus_send(nullptr, reply, nullptr);
        sd_bus_message_unref(reply);
        return r;
    }

    static int onGetProperty(sd_bus_message* m, void*, sd_bus_error*) {
        int32_t id = 0;
        const char* name = nullptr;
        int r = sd_bus_message_read(m, "is", &id, &name);
        if (r < 0) return r;
        const MenuEntry* e = findEntry(id);
        const char* label = (e && std::string_view(name) == "label") ? e->label : "";
        return sd_bus_reply_method_return(m, "v", "s", label);
    }

    static void handleEvent(Tray* tray, int32_t id, const char* eventId) {
        if (std::string_view(eventId) != "clicked") return;
        if (const MenuEntry* e = findEntry(id); e && !e->separator) tray->push(e->action);
    }

    static int onEvent(sd_bus_message* m, void* userdata, sd_bus_error*) {
        int32_t id = 0;
        const char* eventId = nullptr;
        int r = sd_bus_message_read(m, "is", &id, &eventId);
        if (r < 0) return r;
        handleEvent(self(userdata), id, eventId);
        return sd_bus_reply_method_return(m, "");
    }

    static int onEventGroup(sd_bus_message* m, void* userdata, sd_bus_error*) {
        int r = sd_bus_message_enter_container(m, 'a', "(isvu)");
        while (r >= 0 && (r = sd_bus_message_enter_container(m, 'r', "isvu")) > 0) {
            int32_t id = 0;
            const char* eventId = nullptr;
            if (sd_bus_message_read(m, "is", &id, &eventId) >= 0) handleEvent(self(userdata), id, eventId);
            sd_bus_message_skip(m, "vu");
            sd_bus_message_exit_container(m);
        }
        if (r < 0) return r;
        return sd_bus_reply_method_return(m, "ai", 0);
    }

    static int onAboutToShow(sd_bus_message* m, void*, sd_bus_error*) { return sd_bus_reply_method_return(m, "b", 0); }

    static int onAboutToShowGroup(sd_bus_message* m, void*, sd_bus_error*) {
        return sd_bus_reply_method_return(m, "aiai", 0, 0);
    }

    // the panel restarted (or started after us), register again
    static int onNameOwnerChanged(sd_bus_message* m, void* userdata, sd_bus_error*) {
        const char *name = nullptr, *oldOwner = nullptr, *newOwner = nullptr;
        if (sd_bus_message_read(m, "sss", &name, &oldOwner, &newOwner) < 0) return 0;
        if (std::string_view(name) != kWatcher) return 0;
        Tray* tray = self(userdata);
        tray->available_ = false;
        if (newOwner && *newOwner) tray->needRegister_ = true;
        return 0;
    }
};

namespace {

const sd_bus_vtable kItemVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Category", "s", TrayDbus::itemProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Id", "s", TrayDbus::itemProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Title", "s", TrayDbus::itemProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Status", "s", TrayDbus::itemProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("IconName", "s", TrayDbus::itemProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("IconPixmap", "a(iiay)", TrayDbus::itemProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ToolTip", "(sa(iiay)ss)", TrayDbus::itemProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ItemIsMenu", "b", TrayDbus::itemProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Menu", "o", TrayDbus::itemProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("WindowId", "i", TrayDbus::itemProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("Activate", "ii", "", TrayDbus::onActivate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SecondaryActivate", "ii", "", TrayDbus::onActivate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ContextMenu", "ii", "", TrayDbus::onContextMenu, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Scroll", "is", "", TrayDbus::onIgnored, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("NewIcon", "", 0),
    SD_BUS_SIGNAL("NewTitle", "", 0),
    SD_BUS_SIGNAL("NewToolTip", "", 0),
    SD_BUS_SIGNAL("NewStatus", "s", 0),
    SD_BUS_VTABLE_END,
};

const sd_bus_vtable kMenuVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Version", "u", TrayDbus::menuProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("TextDirection", "s", TrayDbus::menuProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Status", "s", TrayDbus::menuProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("IconThemePath", "as", TrayDbus::menuProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("GetLayout", "iias", "u(ia{sv}av)", TrayDbus::onGetLayout, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetGroupProperties", "aias", "a(ia{sv})", TrayDbus::onGetGroupProperties,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetProperty", "is", "v", TrayDbus::onGetProperty, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Event", "isvu", "", TrayDbus::onEvent, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("EventGroup", "a(isvu)", "ai", TrayDbus::onEventGroup, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("AboutToShow", "i", "b", TrayDbus::onAboutToShow, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("AboutToShowGroup", "ai", "aiai", TrayDbus::onAboutToShowGroup, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("ItemsPropertiesUpdated", "a(ia{sv})a(ias)", 0),
    SD_BUS_SIGNAL("LayoutUpdated", "ui", 0),
    SD_BUS_SIGNAL("ItemActivationRequested", "iu", 0),
    SD_BUS_VTABLE_END,
};

}  // namespace

Tray::Tray(std::vector<Icon> icons, std::function<void()> wake) : icons_(std::move(icons)), wake_(std::move(wake)) {
    busName_ = "org.kde.StatusNotifierItem-" + std::to_string(getpid()) + "-1";
    thread_ = std::thread([this] { run(); });
}

Tray::~Tray() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
}

std::vector<Tray::Action> Tray::takeActions() {
    std::lock_guard lock(mutex_);
    return std::exchange(actions_, {});
}

void Tray::push(Action action) {
    {
        std::lock_guard lock(mutex_);
        actions_.push_back(action);
    }
    if (wake_) wake_();
}

bool Tray::registerWithWatcher() {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int r = sd_bus_call_method(bus_, kWatcher, "/StatusNotifierWatcher", kWatcher, "RegisterStatusNotifierItem", &error,
                               nullptr, "s", busName_.c_str());
    sd_bus_error_free(&error);
    return r >= 0;
}

void Tray::run() {
    // sd-bus objects aren't thread safe, so everything bus related lives on this thread
    if (sd_bus_open_user(&bus_) < 0) return;
    bool ok = sd_bus_add_object_vtable(bus_, nullptr, kItemPath, kItemIface, kItemVtable, this) >= 0 &&
              sd_bus_add_object_vtable(bus_, nullptr, kMenuPath, kMenuIface, kMenuVtable, this) >= 0 &&
              sd_bus_request_name(bus_, busName_.c_str(), 0) >= 0 &&
              sd_bus_match_signal(bus_, nullptr, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                  "org.freedesktop.DBus", "NameOwnerChanged", TrayDbus::onNameOwnerChanged, this) >= 0;

    using clock = std::chrono::steady_clock;
    auto lastTry = clock::time_point{};
    while (ok && !stop_) {
        int r = sd_bus_process(bus_, nullptr);
        if (r < 0) break;
        if (r > 0) continue;  // more queued, keep going

        // retry every 2s, at login the panel might not be up yet
        if (needRegister_ && clock::now() - lastTry > std::chrono::seconds(2)) {
            lastTry = clock::now();
            if (registerWithWatcher()) {
                needRegister_ = false;
                available_ = true;
                if (wake_) wake_();
            }
        }
        sd_bus_wait(bus_, 250'000);  // us, also how fast we notice stop_
    }
    available_ = false;
    sd_bus_flush_close_unref(bus_);  // drops our name, the icon disappears
    bus_ = nullptr;
}

}  // namespace fe
