#include "encoder.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
namespace xd {
static void check(int rc, const char *what) {
    if (rc < 0)
        throw std::runtime_error(what);
}
Encoder::Encoder(int w, int h, int fps) {
    try {
        auto codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
        if (!codec)
            throw std::runtime_error("FFmpeg MJPEG encoder missing");
        ctx_ = avcodec_alloc_context3(codec);
        frame_ = av_frame_alloc();
        packet_ = av_packet_alloc();
        if (!ctx_ || !frame_ || !packet_)
            throw std::bad_alloc();
        ctx_->width = w;
        ctx_->height = h;
        ctx_->pix_fmt = AV_PIX_FMT_YUVJ420P;
        ctx_->color_range = AVCOL_RANGE_JPEG;
        ctx_->flags |= AV_CODEC_FLAG_QSCALE;
        ctx_->global_quality = FF_QP2LAMBDA * 4;
        ctx_->time_base = {1, fps};
        ctx_->framerate = {fps, 1};
        ctx_->thread_count = 1;
        check(avcodec_open2(ctx_, codec, nullptr), "encoder open failed");
        frame_->format = ctx_->pix_fmt;
        frame_->color_range = AVCOL_RANGE_JPEG;
        frame_->quality = ctx_->global_quality;
        frame_->width = w;
        frame_->height = h;
        check(av_frame_get_buffer(frame_, 32), "frame allocation failed");
        sws_ = sws_getContext(w, h, AV_PIX_FMT_BGR0, w, h, ctx_->pix_fmt, SWS_FAST_BILINEAR,
                              nullptr, nullptr, nullptr);
        if (!sws_)
            throw std::runtime_error("pixel conversion init failed");
    } catch (...) {
        clear();
        throw;
    }
}
void Encoder::clear() {
    sws_freeContext(sws_);
    av_packet_free(&packet_);
    av_frame_free(&frame_);
    avcodec_free_context(&ctx_);
}
Encoder::~Encoder() { clear(); }
Bytes Encoder::encode(std::span<const uint8_t> pixels) {
    if (pixels.size() != size_t(ctx_->width) * ctx_->height * 4)
        throw std::runtime_error("invalid framebuffer size");
    check(av_frame_make_writable(frame_), "frame not writable");
    const uint8_t *src[] = {pixels.data()};
    int strides[] = {ctx_->width * 4};
    if (sws_scale(sws_, src, strides, 0, ctx_->height, frame_->data, frame_->linesize) !=
        ctx_->height)
        throw std::runtime_error("conversion failed");
    frame_->pts = pts_++;
    check(avcodec_send_frame(ctx_, frame_), "encode failed");
    Bytes out;
    for (;;) {
        int rc = avcodec_receive_packet(ctx_, packet_);
        if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
            break;
        check(rc, "encoded packet failed");
        out.insert(out.end(), packet_->data, packet_->data + packet_->size);
        av_packet_unref(packet_);
    }
    return out;
}
} // namespace xd
