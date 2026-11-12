#pragma once
#include "protocol.hpp"
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;
namespace xd {
class Encoder {
    AVCodecContext *ctx_ = nullptr;
    AVFrame *frame_ = nullptr;
    AVPacket *packet_ = nullptr;
    SwsContext *sws_ = nullptr;
    int64_t pts_ = 0;
    void clear();

  public:
    Encoder(int width, int height, int fps);
    ~Encoder();
    Encoder(const Encoder &) = delete;
    Encoder &operator=(const Encoder &) = delete;
    Bytes encode(std::span<const uint8_t> bgrx);
};
} // namespace xd
