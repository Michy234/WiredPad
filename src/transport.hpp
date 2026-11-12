#pragma once
#include "protocol.hpp"
namespace xd {
class Socket {
    int fd_ = -1;

  public:
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket();
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;
    int fd() const { return fd_; }
    void send(std::span<const uint8_t> data);
    Bytes receive(size_t n, int timeout_ms = 5000);
    Bytes receive_available();
    Packet receive_packet(Order order, int timeout_ms = 5000);
};
struct Session {
    Order order;
    int width, height;
};
Session handshake(Socket &s, bool legacy = false);
int connect_ipad(const char *udid);
} // namespace xd
