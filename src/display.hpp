#pragma once
#include "evdi_lib.h"
#include "protocol.hpp"
namespace xd {
class Display {
    void *lib_ = nullptr;
    evdi_handle handle_ = nullptr;
    int width_, height_;
    bool registered_ = false, pending_ = false, ready_ = false, mode_changed_ = false;
    evdi_mode mode_{};
    Bytes pixels_;
    decltype(&evdi_close) close_ = nullptr;
    decltype(&evdi_connect2) connect_ = nullptr;
    decltype(&evdi_disconnect) disconnect_ = nullptr;
    decltype(&evdi_register_buffer) register_ = nullptr;
    decltype(&evdi_unregister_buffer) unregister_ = nullptr;
    decltype(&evdi_request_update) request_ = nullptr;
    decltype(&evdi_grab_pixels) grab_ = nullptr;
    decltype(&evdi_handle_events) events_ = nullptr;
    decltype(&evdi_get_event_ready) fd_ = nullptr;
    void clear();

  public:
    Display(int card, int width, int height, int fps);
    ~Display();
    Display(const Display &) = delete;
    Display &operator=(const Display &) = delete;
    int fd() const { return fd_(handle_); }
    void events();
    std::span<const uint8_t> capture();
};
} // namespace xd
