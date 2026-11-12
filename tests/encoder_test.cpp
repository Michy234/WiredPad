#include "encoder.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <iostream>
int main() {
    try {
        xd::Encoder encoder(1024, 768, 30);
        xd::Bytes pixels(1024 * 768 * 4, 64);
        auto data = encoder.encode(pixels);
        if (data.size() < 4 || data[0] != 0xff || data[1] != 0xd8 ||
            data[data.size() - 2] != 0xff || data.back() != 0xd9)
            throw std::runtime_error("expected a complete JPEG frame");
        auto decoder = avcodec_alloc_context3(avcodec_find_decoder(AV_CODEC_ID_MJPEG));
        auto packet = av_packet_alloc();
        auto frame = av_frame_alloc();
        if (!decoder || !packet || !frame)
            throw std::bad_alloc();
        int rc = avcodec_open2(decoder, decoder->codec, nullptr);
        if (rc >= 0)
            rc = av_new_packet(packet, data.size());
        if (rc >= 0) {
            std::copy(data.begin(), data.end(), packet->data);
            rc = avcodec_send_packet(decoder, packet);
        }
        if (rc >= 0)
            rc = avcodec_receive_frame(decoder, frame);
        bool ok = rc >= 0 && frame->width == 1024 && frame->height == 768;
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&decoder);
        if (!ok)
            throw std::runtime_error("JPEG frame did not decode at negotiated dimensions");
        std::cout << "JPEG encode/decode passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
