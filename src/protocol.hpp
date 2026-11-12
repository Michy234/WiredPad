#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>
namespace xd {
using Bytes = std::vector<uint8_t>;
enum class Order { Little, Network };
constexpr uint32_t max_payload = 8 * 1024 * 1024;
struct Header {
    uint8_t tag;
    uint32_t length;
};
Header header(std::span<const uint8_t> bytes, Order order);
Bytes packet(uint8_t tag, std::span<const uint8_t> payload, Order order);
uint32_t word(std::span<const uint8_t> bytes, size_t offset, Order order);
Bytes words(std::array<uint32_t, 3> values, Order order);
struct Packet {
    uint8_t tag;
    Bytes payload;
};
class Parser {
    Order order_;
    Bytes data_;
    size_t consumed_ = 0;

  public:
    explicit Parser(Order order) : order_(order) {}
    void feed(std::span<const uint8_t> bytes);
    std::optional<Packet> next();
    void finish() const;
};
struct Touch {
    uint8_t id;
    bool down;
    int x, y;
    bool pointer = false;
};
std::optional<Touch> touch(const Packet &p, int width, int height);
class TouchDecoder {
    bool pointer_down_ = false;

  public:
    std::optional<Touch> decode(const Packet &p, int width, int height);
};
std::array<uint8_t, 128> edid(int width, int height, int fps);
} // namespace xd
