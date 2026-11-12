#include "input.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/uinput.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
namespace xd {
Input::Input(int w, int h, bool pointer) : pointer_(pointer) {
    fd_ = open("/dev/uinput", O_WRONLY | O_CLOEXEC);
    if (fd_ < 0)
        throw std::runtime_error("cannot open /dev/uinput");
    try {
        auto set = [&](unsigned long request, int value) {
            if (ioctl(fd_, request, value) < 0)
                throw std::runtime_error("uinput capability failed");
        };
        set(UI_SET_EVBIT, EV_KEY);
        set(UI_SET_KEYBIT, BTN_TOUCH);
        set(UI_SET_EVBIT, EV_ABS);
        set(UI_SET_PROPBIT, pointer ? INPUT_PROP_POINTER : INPUT_PROP_DIRECT);
        if (pointer) {
            set(UI_SET_KEYBIT, BTN_TOOL_PEN);
            set(UI_SET_KEYBIT, BTN_STYLUS);
        }
        auto axis = [&](int code, int max) {
            set(UI_SET_ABSBIT, code);
            uinput_abs_setup a{};
            a.code = code;
            a.absinfo.maximum = max;
            if (pointer && (code == ABS_X || code == ABS_Y))
                a.absinfo.resolution = 10;
            if (ioctl(fd_, UI_ABS_SETUP, &a) < 0)
                throw std::runtime_error("uinput axis " + std::to_string(code) +
                                         " failed: " + std::strerror(errno));
        };
        axis(ABS_X, w - 1);
        axis(ABS_Y, h - 1);
        if (pointer) {
            axis(ABS_PRESSURE, 255);
            axis(ABS_DISTANCE, 1);
        } else {
            axis(ABS_MT_SLOT, 9);
            axis(ABS_MT_TRACKING_ID, 65535);
            axis(ABS_MT_POSITION_X, w - 1);
            axis(ABS_MT_POSITION_Y, h - 1);
        }
        uinput_setup s{};
        s.id.bustype = BUS_USB;
        s.id.vendor = 0x1209;
        s.id.product = pointer ? 0x5845 : 0x5844;
        std::strcpy(s.name, pointer ? "WiredPad Pointer" : "WiredPad Touch");
        if (ioctl(fd_, UI_DEV_SETUP, &s) < 0 || ioctl(fd_, UI_DEV_CREATE) < 0)
            throw std::runtime_error("uinput create failed");
    } catch (...) {
        close(fd_);
        fd_ = -1;
        throw;
    }
}
Input::~Input() {
    if (fd_ >= 0) {
        ioctl(fd_, UI_DEV_DESTROY);
        close(fd_);
    }
}
void Input::event(uint16_t type, uint16_t code, int value) {
    input_event e{};
    e.type = type;
    e.code = code;
    e.value = value;
    ssize_t n;
    do {
        n = write(fd_, &e, sizeof(e));
    } while (n < 0 && errno == EINTR);
    if (n != sizeof(e))
        throw std::runtime_error("uinput write failed");
}
std::optional<SlotChange> ContactSlots::update(Touch t) {
    auto slot = std::find(ids_.begin(), ids_.end(), int(t.id));
    bool begins = false;
    if (slot == ids_.end()) {
        if (!t.down)
            return {};
        slot = std::find(ids_.begin(), ids_.end(), -1);
        if (slot == ids_.end())
            throw std::runtime_error("more than 10 simultaneous contacts");
        *slot = t.id;
        begins = true;
    }
    int index = int(slot - ids_.begin());
    if (!t.down)
        *slot = -1;
    bool active = std::any_of(ids_.begin(), ids_.end(), [](int id) { return id >= 0; });
    return SlotChange{index, begins, active};
}
void Input::apply(Touch t) {
    if (pointer_) {
        if (!proximity_) {
            event(EV_ABS, ABS_X, t.x);
            event(EV_ABS, ABS_Y, t.y);
            event(EV_KEY, BTN_TOOL_PEN, 1);
            event(EV_SYN, SYN_REPORT, 0);
            proximity_ = true;
        }
        event(EV_ABS, ABS_X, t.x);
        event(EV_ABS, ABS_Y, t.y);
        event(EV_ABS, ABS_PRESSURE, t.down ? 255 : 0);
        event(EV_KEY, BTN_TOUCH, t.down);
        event(EV_SYN, SYN_REPORT, 0);
        return;
    }
    auto change = slots_.update(t);
    if (!change)
        return;
    event(EV_ABS, ABS_MT_SLOT, change->slot);
    if (t.down) {
        if (change->begins)
            event(EV_ABS, ABS_MT_TRACKING_ID, t.id);
        event(EV_ABS, ABS_MT_POSITION_X, t.x);
        event(EV_ABS, ABS_MT_POSITION_Y, t.y);
        event(EV_ABS, ABS_X, t.x);
        event(EV_ABS, ABS_Y, t.y);
    } else
        event(EV_ABS, ABS_MT_TRACKING_ID, -1);
    event(EV_KEY, BTN_TOUCH, change->any_active);
    event(EV_SYN, SYN_REPORT, 0);
}
} // namespace xd
