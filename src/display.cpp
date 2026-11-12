#include "display.hpp"
#include <dlfcn.h>
#include <iostream>
namespace xd {
Display::Display(int card, int w, int h, int fps) : width_(w), height_(h) {
    try {
        lib_ = dlopen("libevdi.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!lib_)
            lib_ = dlopen("libevdi.so", RTLD_NOW | RTLD_LOCAL);
        if (!lib_)
            throw std::runtime_error("install EVDI library and matching evdi kernel module");
#define LOAD(var, name)                                                                            \
    var = reinterpret_cast<decltype(var)>(dlsym(lib_, name));                                      \
    if (!var)                                                                                      \
    throw std::runtime_error("EVDI symbol missing: " name)
        decltype(&evdi_open) open_;
        LOAD(open_, "evdi_open");
        LOAD(close_, "evdi_close");
        LOAD(connect_, "evdi_connect2");
        LOAD(disconnect_, "evdi_disconnect");
        LOAD(register_, "evdi_register_buffer");
        LOAD(unregister_, "evdi_unregister_buffer");
        LOAD(request_, "evdi_request_update");
        LOAD(grab_, "evdi_grab_pixels");
        LOAD(events_, "evdi_handle_events");
        LOAD(fd_, "evdi_get_event_ready");
#undef LOAD
        handle_ = open_(card);
        if (!handle_)
            throw std::runtime_error(
                "cannot open EVDI card (wrong card, permissions, or already in use)");
        auto e = edid(w, h, fps);
        connect_(handle_, e.data(), e.size(), uint32_t(w * h), uint32_t(w * h * fps));
    } catch (...) {
        clear();
        throw;
    }
}
void Display::clear() {
    if (handle_) {
        if (registered_)
            unregister_(handle_, 1);
        disconnect_(handle_);
        close_(handle_);
        handle_ = nullptr;
    }
    if (lib_) {
        dlclose(lib_);
        lib_ = nullptr;
    }
}
Display::~Display() { clear(); }
void Display::events() {
    evdi_event_context context{};
    context.user_data = this;
    context.mode_changed_handler = [](evdi_mode m, void *self) {
        auto d = static_cast<Display *>(self);
        d->mode_ = m;
        d->mode_changed_ = true;
    };
    context.update_ready_handler = [](int id, void *self) {
        if (id == 1)
            static_cast<Display *>(self)->ready_ = true;
    };
    events_(handle_, &context);
    if (mode_changed_) {
        mode_changed_ = false;
        if (registered_) {
            unregister_(handle_, 1);
            registered_ = false;
        }
        pending_ = ready_ = false;
        if (mode_.width != width_ || mode_.height != height_) {
            std::cerr << "Enable EVDI output at " << width_ << "x" << height_
                      << " in display settings (current " << mode_.width << "x" << mode_.height
                      << ")\n";
            return;
        }
        if (mode_.bits_per_pixel != 32 ||
            (mode_.pixel_format != 0x34325258 && mode_.pixel_format != 0x34325241)) {
            std::cerr << "EVDI mode " << mode_.width << "x" << mode_.height
                      << " bpp=" << mode_.bits_per_pixel << " format=0x" << std::hex
                      << mode_.pixel_format << std::dec << std::endl;
            throw std::runtime_error("EVDI requires XRGB8888 or ARGB8888 pixels");
        }
        pixels_.assign(size_t(width_) * height_ * 4, 0);
        evdi_buffer b{};
        b.id = 1;
        b.buffer = pixels_.data();
        b.width = width_;
        b.height = height_;
        b.stride = width_ * 4;
        register_(handle_, b);
        registered_ = true;
    }
}
std::span<const uint8_t> Display::capture() {
    if (!registered_)
        return {};
    if (!pending_) {
        ready_ = request_(handle_, 1);
        pending_ = true;
    }
    if (!ready_)
        return {};
    evdi_rect rects[16]{};
    int count = 16;
    grab_(handle_, rects, &count);
    ready_ = pending_ = false;
    if (count == 0)
        return {};
    return pixels_;
}
} // namespace xd
