#include "protocol.hpp"
#include <algorithm>
namespace xd {
Header header(std::span<const uint8_t> b, Order o) {
    if (b.size() < 4)
        throw std::runtime_error("truncated TLV header");
    uint32_t n = o == Order::Little ? uint32_t(b[1]) | uint32_t(b[2]) << 8 | uint32_t(b[3]) << 16
                                    : uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
    if (n > max_payload)
        throw std::runtime_error("TLV payload exceeds 8 MiB");
    return {b[0], n};
}
Bytes packet(uint8_t tag, std::span<const uint8_t> p, Order o) {
    if (p.size() > max_payload)
        throw std::runtime_error("payload too large");
    auto n = uint32_t(p.size());
    Bytes b{tag};
    if (o == Order::Little)
        b.insert(b.end(), {uint8_t(n), uint8_t(n >> 8), uint8_t(n >> 16)});
    else
        b.insert(b.end(), {uint8_t(n >> 16), uint8_t(n >> 8), uint8_t(n)});
    b.insert(b.end(), p.begin(), p.end());
    return b;
}
uint32_t word(std::span<const uint8_t> b, size_t i, Order o) {
    if (i > b.size() || b.size() - i < 4)
        throw std::runtime_error("truncated u32");
    uint32_t v = 0;
    for (int k = 0; k < 4; k++)
        v |= uint32_t(b[i + k]) << (8 * (o == Order::Little ? k : 3 - k));
    return v;
}
Bytes words(std::array<uint32_t, 3> v, Order o) {
    Bytes b;
    for (auto x : v)
        for (int k = 0; k < 4; k++)
            b.push_back(uint8_t(x >> (8 * (o == Order::Little ? k : 3 - k))));
    return b;
}
void Parser::feed(std::span<const uint8_t> b) {
    if (consumed_) {
        data_.erase(data_.begin(), data_.begin() + consumed_);
        consumed_ = 0;
    }
    if (b.size() > max_payload + 4 - data_.size())
        throw std::runtime_error("receive buffer exceeds limit");
    data_.insert(data_.end(), b.begin(), b.end());
}
std::optional<Packet> Parser::next() {
    auto b = std::span<const uint8_t>(data_).subspan(consumed_);
    if (b.size() < 4)
        return {};
    auto h = header(b, order_);
    if (b.size() < 4 + h.length)
        return {};
    Packet p{h.tag, Bytes(b.begin() + 4, b.begin() + 4 + h.length)};
    consumed_ += 4 + h.length;
    return p;
}
void Parser::finish() const {
    if (data_.size() != consumed_)
        throw std::runtime_error("EOF inside TLV packet");
}
std::optional<Touch> touch(const Packet &p, int w, int h) {
    if (p.tag != 3 && p.tag != 7)
        return {};
    if (p.payload.size() != 12)
        throw std::runtime_error("input payload must contain three u32");
    auto o = p.tag == 7 ? Order::Network : Order::Little;
    auto type = word(p.payload, 0, o), id = word(p.payload, 4, o), xy = word(p.payload, 8, o);
    if (type != 16 && type != 17)
        return {};
    if (id > 255)
        throw std::runtime_error("unsupported touch ID bits");
    // Agent 0x440540: signed x at +8 minus one, signed y at +10.
    int x = int(int16_t(xy & 0xffff)) - 1, y = int(int16_t(xy >> 16));
    return Touch{uint8_t(id), type == 16, std::clamp(x, 0, w - 1), std::clamp(y, 0, h - 1)};
}
std::optional<Touch> TouchDecoder::decode(const Packet &p, int w, int h) {
    if (w <= 0 || h <= 0)
        throw std::runtime_error("invalid input dimensions");
    if (auto hid = touch(p, w, h))
        return hid;
    if (p.tag != 3 && p.tag != 7)
        return {};
    auto order = p.tag == 7 ? Order::Network : Order::Little;
    auto type = word(p.payload, 0, order);
    if (type == 5)
        pointer_down_ = true;
    else if (type == 6)
        pointer_down_ = false;
    else if (type != 10)
        return {};
    auto x = word(p.payload, 4, order), y = word(p.payload, 8, order);
    return Touch{0, pointer_down_, int(std::min(x, uint32_t(w - 1))),
                 int(std::min(y, uint32_t(h - 1))), true};
}

std::array<uint8_t, 128> edid(int w, int h, int fps) {
    if (w < 320 || w > 4095 || h < 200 || h > 4095 || fps < 1 || fps > 60)
        throw std::runtime_error("invalid display mode");
    std::array<uint8_t, 128> e{};
    const uint8_t base[] = {0, 255, 255, 255, 255, 255, 255, 0,    0x61, 0x98, 1,    0,   0,
                            0, 0,   0,   1,   36,  1,   3,   0x80, 0,    0,    0x78, 0x0a};
    std::copy(std::begin(base), std::end(base), e.begin());
    e[24] = 0x0e;
    const uint8_t chroma[] = {0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26, 0x0f, 0x50, 0x54};
    std::copy(std::begin(chroma), std::end(chroma), e.begin() + 25);
    for (int i = 38; i < 54; i += 2)
        e[i] = e[i + 1] = 1;
    const int hb = 160, vb = 45, clock = ((w + hb) * (h + vb) * fps + 5000) / 10000;
    if (clock > 65535)
        throw std::runtime_error("EDID pixel clock too high");
    auto d = e.data() + 54;
    d[0] = uint8_t(clock);
    d[1] = uint8_t(clock >> 8);
    d[2] = uint8_t(w);
    d[3] = hb;
    d[4] = uint8_t((w >> 8) << 4);
    d[5] = uint8_t(h);
    d[6] = vb;
    d[7] = uint8_t((h >> 8) << 4);
    d[8] = 48;
    d[9] = 32;
    d[10] = 0x35;
    d[17] = 0x1a;
    auto name = e.data() + 72;
    name[3] = 0xfc;
    const char label[] = "XDisplay iOS\n";
    std::copy(label, label + 13, name + 5);
    auto range = e.data() + 90;
    range[3] = 0xfd;
    range[5] = std::max(1, fps - 1);
    range[6] = fps + 1;
    range[7] = std::max(1, clock * 10 / (w + hb));
    range[8] = range[7] + 1;
    range[9] = (clock + 999) / 1000;
    range[10] = 0;
    range[11] = 0x0a;
    std::fill(range + 12, range + 18, 0x20);
    e[111] = 0x10;
    unsigned sum = 0;
    for (int i = 0; i < 127; i++)
        sum += e[i];
    e[127] = uint8_t(-sum);
    return e;
}
} // namespace xd
