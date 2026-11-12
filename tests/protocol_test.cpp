#include "input.hpp"
#include "protocol.hpp"
#include "transport.hpp"
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error("failed: " #x);                                               \
    } while (0)
template <class F> void rejects(F f) {
    bool failed = false;
    try {
        f();
    } catch (const std::exception &) {
        failed = true;
    }
    CHECK(failed);
}
using namespace xd;
void parser_tests(Order o) {
    auto payload = words({2, 2048, 1536}, o);
    auto b = packet(o == Order::Little ? 1 : 5, payload, o);
    CHECK(b.size() == 16);
    CHECK(b[1] == (o == Order::Little ? 12 : 0));
    CHECK(b[3] == (o == Order::Little ? 0 : 12));
    for (size_t split = 0; split <= b.size(); split++) {
        Parser p(o);
        p.feed(std::span(b).first(split));
        auto first = p.next();
        if (split < b.size())
            CHECK(!first);
        p.feed(std::span(b).subspan(split));
        auto last = split == b.size() ? first : p.next();
        CHECK(last);
        CHECK(word(last->payload, 4, o) == 2048);
        CHECK(!p.next());
        p.finish();
    }
    Parser p(o);
    auto twice = b;
    twice.insert(twice.end(), b.begin(), b.end());
    p.feed(twice);
    CHECK(p.next());
    CHECK(p.next());
    CHECK(!p.next());
    p.finish();
    Parser truncated(o);
    truncated.feed(std::span(b).first(15));
    CHECK(!truncated.next());
    rejects([&] { truncated.finish(); });
    rejects([&] { header(Bytes{1, 255, 255, 255}, o); });
    rejects([&] { word(Bytes{1, 2, 3}, 0, o); });
    Packet touch_packet{uint8_t(o == Order::Network ? 7 : 3),
                        words({16, 5, (200u << 16) | 101}, o)};
    auto t = touch(touch_packet, 1024, 768);
    CHECK(t && !t->pointer && t->id == 5 && t->x == 100 && t->y == 200 && t->down);
    touch_packet.payload = words({17, 5, 0xffffffff}, o);
    t = touch(touch_packet, 1024, 768);
    CHECK(t && !t->down && t->x == 0 && t->y == 0);
    touch_packet.payload.resize(11);
    rejects([&] { touch(touch_packet, 1024, 768); });
    touch_packet.payload = words({16, 256, 0}, o);
    rejects([&] { touch(touch_packet, 1024, 768); });
}
void legacy_touch_tests(Order order) {
    TouchDecoder decoder;
    auto input = [&](uint32_t cmd, uint32_t x, uint32_t y) {
        return Packet{uint8_t(order == Order::Network ? 7 : 3), words({cmd, x, y}, order)};
    };
    auto hover = decoder.decode(input(10, 433, 334), 1024, 768);
    CHECK(hover && hover->pointer && !hover->down);
    auto down = decoder.decode(input(5, 433, 334), 1024, 768);
    CHECK(down && down->pointer && down->id == 0 && down->down && down->x == 433 && down->y == 334);
    auto move = decoder.decode(input(10, 500, 400), 1024, 768);
    CHECK(move && move->down && move->x == 500 && move->y == 400);
    auto up = decoder.decode(input(6, 500, 400), 1024, 768);
    CHECK(up && !up->down);
    hover = decoder.decode(input(10, 600, 500), 1024, 768);
    CHECK(hover && hover->pointer && !hover->down);
    auto edge = decoder.decode(input(5, 0xffffffff, 0xffffffff), 1024, 768);
    CHECK(edge && edge->x == 1023 && edge->y == 767);
    CHECK(!decoder.decode(input(8, 0, 0), 1024, 768));
}

void slot_tests() {
    ContactSlots slots;
    CHECK(!slots.update({255, false, 0, 0}));
    auto first = slots.update({255, true, 1, 2});
    CHECK(first && first->slot == 0 && first->begins && first->any_active);
    auto move = slots.update({255, true, 3, 4});
    CHECK(move && move->slot == 0 && !move->begins);
    for (int i = 0; i < 9; i++)
        CHECK(slots.update({uint8_t(i), true, 0, 0}));
    rejects([&] { slots.update({100, true, 0, 0}); });
    auto up = slots.update({255, false, 3, 4});
    CHECK(up && up->slot == 0 && up->any_active);
    auto reused = slots.update({100, true, 0, 0});
    CHECK(reused && reused->slot == 0 && reused->begins);
    for (int i = 0; i < 9; i++)
        CHECK(slots.update({uint8_t(i), false, 0, 0}));
    auto last = slots.update({100, false, 0, 0});
    CHECK(last && !last->any_active);
}

void handshake_test(Order o, bool forced_legacy = false) {
    int fds[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    std::exception_ptr failure;
    std::thread peer([&] {
        try {
            Socket s(fds[1]);
            if (!forced_legacy)
                CHECK(s.receive(4) == Bytes({4, 0, 0, 0}));
            if (o == Order::Network)
                s.send(Bytes{4, 0, 0, 0});
            auto hello = s.receive_packet(o, 7000);
            CHECK(hello.tag == (o == Order::Network ? 5 : 1));
            CHECK(word(hello.payload, 0, o) == 1);
            auto ack = packet(o == Order::Network ? 5 : 1, words({2, 2048, 1536}, o), o);
            for (auto c : ack)
                s.send(Bytes{c});
            auto ready = s.receive_packet(o);
            CHECK(word(ready.payload, 0, o) == 254);
            s.send(packet(o == Order::Network ? 5 : 1, words({255, 0, 0}, o), o));
            auto feature = s.receive_packet(o);
            CHECK(feature.tag == (o == Order::Network ? 5 : 1));
            CHECK(word(feature.payload, 0, o) == 252);
            CHECK(word(feature.payload, 4, Order::Little) == 5);
            s.send(packet(o == Order::Network ? 5 : 1, words({253, 0, 0}, o), o));
        } catch (...) {
            failure = std::current_exception();
        }
    });
    std::exception_ptr host_failure;
    try {
        Socket host(fds[0]);
        auto s = handshake(host, forced_legacy);
        CHECK(s.order == o && s.width == 2048 && s.height == 1536);
    } catch (...) {
        host_failure = std::current_exception();
    }
    peer.join();
    if (failure)
        std::rethrow_exception(failure);
    if (host_failure)
        std::rethrow_exception(host_failure);
}
int main() {
    try {
        parser_tests(Order::Little);
        parser_tests(Order::Network);
        legacy_touch_tests(Order::Little);
        legacy_touch_tests(Order::Network);
        slot_tests();
        handshake_test(Order::Network);
        handshake_test(Order::Little);
        handshake_test(Order::Little, true);
        auto e = edid(2048, 1536, 30);
        unsigned sum = 0;
        for (auto b : e)
            sum += b;
        CHECK((sum & 255) == 0);
        CHECK(e[126] == 0);
        rejects([] { edid(0, 1536, 30); });
        int fd[2];
        CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
        Socket s(fd[0]);
        close(fd[1]);
        rejects([&] { s.receive(4, 100); });
        std::cout << "Parser, input, EDID, fragmented handshake and EOF tests passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
