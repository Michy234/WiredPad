#include "display.hpp"
#include "encoder.hpp"
#include "input.hpp"
#include "platform.hpp"
#include "transport.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>
#include <poll.h>
#include <thread>
#include <unistd.h>
namespace {
volatile sig_atomic_t stopped = 0;
void stop(int) { stopped = 1; }
int number(const char *s, int lo, int hi) {
    int n;
    auto end = s + std::char_traits<char>::length(s);
    auto [p, err] = std::from_chars(s, end, n);
    if (err != std::errc{} || p != end || n < lo || n > hi)
        throw std::runtime_error("invalid numeric argument");
    return n;
}
} // namespace
int main(int argc, char **argv) {
    try {
        bool probe = false, no_input = false, test_pattern = false, legacy = false,
             trace_input = false;
        int card = -1, fps = 30;
        const char *udid = nullptr;
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];
            if (a == "--help") {
                std::cout << "wiredpad-host --probe | --test-pattern | --evdi-card N [--fps 1..60] "
                             "[--udid UDID] "
                             "[--no-input] [--legacy] [--trace-input]\n";
                return 0;
            }
            if (a == "--probe")
                probe = true;
            else if (a == "--test-pattern")
                test_pattern = true;
            else if (a == "--trace-input")
                trace_input = true;
            else if (a == "--legacy")
                legacy = true;
            else if (a == "--no-input")
                no_input = true;
            else if (a == "--evdi-card" && i + 1 < argc)
                card = number(argv[++i], 0, 255);
            else if (a == "--fps" && i + 1 < argc)
                fps = number(argv[++i], 1, 60);
            else if (a == "--udid" && i + 1 < argc)
                udid = argv[++i];
            else
                throw std::runtime_error("unknown or incomplete argument; see --help");
        }
        if (!probe && !test_pattern && card < 0) {
            auto cards = xd::evdi_cards();
            if (cards.empty())
                throw std::runtime_error("no EVDI display found; load the evdi module first");
            if (cards.size() != 1)
                throw std::runtime_error("multiple EVDI displays found; select --evdi-card N");
            card = cards.front();
        }
        auto lock_path = std::filesystem::path("/run/user") / std::to_string(getuid());
        if (std::filesystem::is_directory(lock_path))
            lock_path /= "wiredpad-host.lock";
        else
            lock_path = std::filesystem::path("/tmp") /
                        ("wiredpad-host-" + std::to_string(getuid()) + ".lock");
        xd::HostLock lock(lock_path);
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);
        while (!stopped) {
            try {
                int fd = xd::connect_ipad(udid);
                if (fd < 0) {
                    if (probe)
                        throw std::runtime_error(
                            "no paired USB iPad with XDisplay listening on port 6789");
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    continue;
                }
                xd::Socket socket(fd);
                auto s = xd::handshake(socket, legacy);
                std::cout << "iPad " << s.width << "x" << s.height << " byte-order="
                          << (s.order == xd::Order::Network ? "network" : "little") << std::endl;
                if (probe)
                    return 0;
                std::unique_ptr<xd::Display> display;
                if (!test_pattern)
                    display = std::make_unique<xd::Display>(card, s.width, s.height, fps);
                xd::Bytes pattern(test_pattern ? size_t(s.width) * s.height * 4 : 0);
                uint64_t frames = 0, video_bytes = 0;
                auto report = std::chrono::steady_clock::now();
                xd::Encoder encoder(s.width, s.height, fps);
                std::unique_ptr<xd::Input> input, pointer;
                if (!no_input && !test_pattern) {
                    input = std::make_unique<xd::Input>(s.width, s.height);
                    pointer = std::make_unique<xd::Input>(s.width, s.height, true);
                }
                xd::Parser parser(s.order);
                xd::TouchDecoder touch_decoder;
                auto next = std::chrono::steady_clock::now();
                while (!stopped) {
                    auto now = std::chrono::steady_clock::now();
                    int ms = std::max(
                        0, int(std::chrono::ceil<std::chrono::milliseconds>(next - now).count()));
                    pollfd fds[] = {{socket.fd(), POLLIN, 0},
                                    {display ? display->fd() : -1, POLLIN, 0}};
                    int rc = poll(fds, 2, ms);
                    if (rc < 0) {
                        if (errno == EINTR)
                            continue;
                        throw std::runtime_error("session poll failed");
                    }
                    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
                        throw std::runtime_error("iPad disconnected");
                    if (fds[0].revents & POLLIN) {
                        // Read bounded chunks while preserving TLV fragmentation/coalescing.
                        auto b = socket.receive_available();
                        parser.feed(b);
                        while (auto p = parser.next()) {
                            if ((test_pattern || trace_input) && p->payload.size() == 12) {
                                auto o = p->tag == 7 ? xd::Order::Network : xd::Order::Little;
                                std::cout << "client tag=" << unsigned(p->tag)
                                          << " words=" << xd::word(p->payload, 0, o) << ","
                                          << xd::word(p->payload, 4, o) << ","
                                          << xd::word(p->payload, 8, o) << std::endl;
                            }
                            if (auto t = touch_decoder.decode(*p, s.width, s.height); t) {
                                if (t->pointer && pointer)
                                    pointer->apply(*t);
                                else if (!t->pointer && input)
                                    input->apply(*t);
                                if (test_pattern || trace_input)
                                    std::cout << "touch id=" << unsigned(t->id)
                                              << " down=" << t->down << " x=" << t->x
                                              << " y=" << t->y << std::endl;
                            } else if ((p->tag == 3 || p->tag == 7) && p->payload.size() == 12) {
                                auto o = p->tag == 7 ? xd::Order::Network : xd::Order::Little;
                                auto cmd = xd::word(p->payload, 0, o);
                                if (cmd == 0x70 || cmd == 0x71)
                                    throw std::runtime_error(
                                        "iPad requests video reconfiguration; reconnecting");
                            }
                        }
                    }
                    if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL))
                        throw std::runtime_error("EVDI disconnected");
                    if (fds[1].revents & POLLIN)
                        display->events();
                    now = std::chrono::steady_clock::now();
                    if (now >= next) {
                        std::span<const uint8_t> pixels;
                        if (test_pattern) {
                            const int marker = int((frames * 4) % s.width);
                            for (int y = 0; y < s.height; ++y) {
                                for (int x = 0; x < s.width; ++x) {
                                    size_t i = (size_t(y) * s.width + x) * 4;
                                    bool bar = x >= marker && x < marker + 16;
                                    pattern[i] = bar ? 255 : (x < s.width / 2 ? 32 : 220);
                                    pattern[i + 1] = bar ? 255 : (y < s.height / 2 ? 32 : 180);
                                    pattern[i + 2] = bar ? 255 : (x < s.width / 2 ? 220 : 32);
                                }
                            }
                            pixels = pattern;
                        } else {
                            pixels = display->capture();
                        }
                        if (!pixels.empty()) {
                            auto video = encoder.encode(pixels);
                            if (!video.empty()) {
                                socket.send(xd::packet(s.order == xd::Order::Network ? 6 : 2, video,
                                                       s.order));
                                ++frames;
                                video_bytes += video.size();
                            }
                        }
                        if (now - report >= std::chrono::seconds(5)) {
                            std::cout << "stream frames=" << frames << " bytes=" << video_bytes
                                      << std::endl;
                            report = now;
                        }
                        next = now + std::chrono::microseconds(1000000 / fps);
                    }
                }
            } catch (const std::exception &e) {
                if (probe)
                    throw;
                std::cerr << e.what() << "; retry in 2 seconds\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "wiredpad-host: " << e.what() << '\n';
        return 1;
    }
}
