#pragma once
#include "protocol.hpp"
namespace xd {
struct SlotChange {
    int slot;
    bool begins, any_active;
};
class ContactSlots {
    std::array<int, 10> ids_;

  public:
    ContactSlots() { ids_.fill(-1); }
    std::optional<SlotChange> update(Touch touch);
};
class Input {
    int fd_ = -1;
    ContactSlots slots_;
    bool pointer_ = false, proximity_ = false;
    void event(uint16_t type, uint16_t code, int value);

  public:
    Input(int width, int height, bool pointer = false);
    ~Input();
    Input(const Input &) = delete;
    Input &operator=(const Input &) = delete;
    void apply(Touch t);
};
} // namespace xd
