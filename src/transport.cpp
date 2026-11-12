#include "transport.hpp"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <plist/plist.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <usbmuxd.h>
namespace xd {
Socket::~Socket() {
    if (fd_ >= 0)
        close(fd_);
}
static void wait_fd(int fd, short events, std::chrono::steady_clock::time_point deadline) {
    for (;;) {
        int ms = int(std::chrono::duration_cast<std::chrono::milliseconds>(
                         deadline - std::chrono::steady_clock::now())
                         .count());
        if (ms <= 0)
            throw std::runtime_error("USB I/O timeout");
        pollfd p{fd, events, 0};
        int r = poll(&p, 1, ms);
        if (r < 0 && errno == EINTR)
            continue;
        if (r == 0)
            throw std::runtime_error("USB I/O timeout");
        if (r < 0)
            throw std::runtime_error(std::strerror(errno));
        if (p.revents & events)
            return;
        throw std::runtime_error("USB disconnected");
    }
}
void Socket::send(std::span<const uint8_t> b) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!b.empty()) {
        wait_fd(fd_, POLLOUT, deadline);
        auto n = ::send(fd_, b.data(), b.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
            continue;
        if (n <= 0)
            throw std::runtime_error("USB write failed");
        b = b.subspan(size_t(n));
    }
}
Bytes Socket::receive(size_t n, int timeout) {
    if (n > max_payload)
        throw std::runtime_error("read exceeds limit");
    Bytes b(n);
    size_t pos = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    while (pos < n) {
        wait_fd(fd_, POLLIN, deadline);
        auto r = recv(fd_, b.data() + pos, n - pos, MSG_DONTWAIT);
        if (r < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
            continue;
        if (r <= 0)
            throw std::runtime_error("EOF inside packet");
        pos += size_t(r);
    }
    return b;
}
Bytes Socket::receive_available() {
    Bytes b(65536);
    ssize_t n;
    do {
        n = recv(fd_, b.data(), b.size(), MSG_DONTWAIT);
    } while (n < 0 && errno == EINTR);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return {};
    if (n <= 0)
        throw std::runtime_error("iPad EOF");
    b.resize(size_t(n));
    return b;
}
Packet Socket::receive_packet(Order o, int ms) {
    auto h = header(receive(4, ms), o);
    return {h.tag, receive(h.length, ms)};
}
Session handshake(Socket &s, bool legacy) {
    if (!legacy)
        s.send(Bytes{4, 0, 0, 0});
    Order o = Order::Little;
    // A timeout with no bytes is the legacy fallback. Partial replies are fatal.
    pollfd p{s.fd(), POLLIN, 0};
    int r;
    do {
        r = legacy ? 0 : poll(&p, 1, 5000);
    } while (r < 0 && errno == EINTR);
    if (r < 0)
        throw std::runtime_error("poll handshake failed");
    if (r > 0) {
        auto ack = s.receive(4);
        if (ack[0] == 4)
            o = Order::Network;
        else
            throw std::runtime_error("unexpected byte-order ACK");
    }
    auto command = [&](uint32_t cmd) {
        s.send(packet(o == Order::Network ? 5 : 1, words({cmd, 0, 0}, o), o));
    };
    command(1);
    auto hello = s.receive_packet(o);
    if (hello.tag != (o == Order::Network ? 5 : 1) || hello.payload.size() != 12 ||
        word(hello.payload, 0, o) != 2)
        throw std::runtime_error("invalid HELLO ACK");
    auto w = word(hello.payload, 4, o), h = word(hello.payload, 8, o);
    if (w < 320 || w > 4092 || h < 200 || h > 4092)
        throw std::runtime_error("invalid iPad resolution");
    command(254);
    auto ready = s.receive_packet(o);
    if (ready.tag != (o == Order::Network ? 5 : 1) || ready.payload.size() != 12 ||
        word(ready.payload, 0, o) != 255)
        throw std::runtime_error("invalid READY ACK");
    // The Windows agent writes the feature mask in little endian, even after
    // negotiating network byte order. Keep that observed wire representation.
    auto feature = words({252, 0, 0}, o);
    feature[4] = 5;
    s.send(packet(o == Order::Network ? 5 : 1, feature, o));
    p = {s.fd(), POLLIN, 0};
    do {
        r = poll(&p, 1, 500);
    } while (r < 0 && errno == EINTR);
    if (r < 0)
        throw std::runtime_error("poll capabilities failed");
    if (r > 0 && (p.revents & POLLIN)) {
        auto extra = s.receive_packet(o);
        std::cerr << "client capabilities tag=" << unsigned(extra.tag)
                  << " bytes=" << extra.payload.size();
        if (extra.payload.size() == 12 && (extra.tag == 1 || extra.tag == 5))
            std::cerr << " command="
                      << word(extra.payload, 0, extra.tag == 5 ? Order::Network : Order::Little);
        std::cerr << std::endl;
    }
    return {o, int((w + 3) & ~3u), int((h + 3) & ~3u)};
}
int connect_ipad(const char *requested) {
    usbmuxd_device_info_t *list = nullptr;
    int n = usbmuxd_get_device_list(&list);
    if (n < 0)
        throw std::runtime_error("usbmuxd unavailable; start usbmuxd.service");
    int fd = -1;
    for (int i = 0; i < n; i++) {
        auto &d = list[i];
        if (d.conn_type != CONNECTION_TYPE_USB || (requested && std::strcmp(requested, d.udid)))
            continue;
        idevice_t device = nullptr;
        lockdownd_client_t client = nullptr;
        plist_t value = nullptr;
        if (idevice_new_with_options(&device, d.udid, IDEVICE_LOOKUP_USBMUX) != IDEVICE_E_SUCCESS)
            continue;
        auto err = lockdownd_client_new_with_handshake(device, &client, "wiredpad");
        bool ipad = false;
        if (err == LOCKDOWN_E_SUCCESS &&
            lockdownd_get_value(client, nullptr, "ProductType", &value) == LOCKDOWN_E_SUCCESS &&
            value) {
            const char *type = plist_get_string_ptr(value, nullptr);
            ipad = type && std::strncmp(type, "iPad", 4) == 0;
        }
        if (value)
            plist_free(value);
        if (client)
            lockdownd_client_free(client);
        idevice_free(device);
        if (!ipad)
            continue;
        fd = usbmuxd_connect(d.handle, 6789);
        if (fd >= 0)
            break;
    }
    usbmuxd_device_list_free(&list);
    return fd;
}
} // namespace xd
