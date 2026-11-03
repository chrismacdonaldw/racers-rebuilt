#include "video/video_decoder.hpp"

#include <array>
#include <cstring>
#include <format>
#include <string_view>
#include <utility>

#include "video/yuv.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

namespace rr::video {
namespace {

constexpr std::uint32_t kIndeo5 = assets::FourCc("IV50");
constexpr std::uint32_t kLowerCaseBits = 0x20202020U;
constexpr std::size_t kRgbaBytes = 4;

struct ContextDeleter {
    void operator()(AVCodecContext* context) const { avcodec_free_context(&context); }
};

struct FrameDeleter {
    void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};

struct PacketDeleter {
    void operator()(AVPacket* packet) const { av_packet_free(&packet); }
};

Error CodecError(std::string_view step, int code) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> text{};
    av_strerror(code, text.data(), text.size());
    return {.code = ErrorCode::InvalidData, .message = std::format("{}: {}", step, text.data())};
}

std::span<const std::uint8_t> PlaneOf(const AVFrame& frame, std::size_t plane, std::size_t rows) {
    const auto stride = static_cast<std::size_t>(frame.linesize[plane]);
    return {frame.data[plane], stride * rows};
}

}

struct VideoDecoder::Codec {
    std::unique_ptr<AVCodecContext, ContextDeleter> context;
    std::unique_ptr<AVFrame, FrameDeleter> frame;
    std::unique_ptr<AVPacket, PacketDeleter> packet;
};

Result<VideoDecoder> VideoDecoder::Create(const assets::AviVideo& video) {
    if ((video.codec | kLowerCaseBits) != (kIndeo5 | kLowerCaseBits)) {
        return Fail(ErrorCode::NotFound,
                    std::format("video codec {:08x} isn't Indeo 5", video.codec));
    }
    const AVCodec* decoder = avcodec_find_decoder(AV_CODEC_ID_INDEO5);
    auto codec = std::make_unique<Codec>(Codec{
        .context = std::unique_ptr<AVCodecContext, ContextDeleter>(avcodec_alloc_context3(decoder)),
        .frame = std::unique_ptr<AVFrame, FrameDeleter>(av_frame_alloc()),
        .packet = std::unique_ptr<AVPacket, PacketDeleter>(av_packet_alloc())});
    if (decoder == nullptr || !codec->context || !codec->frame || !codec->packet) {
        return Fail(ErrorCode::Platform, "can't set up the Indeo 5 decoder");
    }
    codec->context->width = static_cast<int>(video.width);
    codec->context->height = static_cast<int>(video.height);
    codec->context->thread_count = 1;
    if (const int opened = avcodec_open2(codec->context.get(), decoder, nullptr); opened < 0) {
        return std::unexpected(CodecError("can't open the Indeo 5 decoder", opened));
    }
    return VideoDecoder(std::move(codec), video.width, video.height);
}

VideoDecoder::VideoDecoder(std::unique_ptr<Codec> codec, std::uint32_t width, std::uint32_t height)
    : codec_(std::move(codec)),
      width_(width),
      height_(height),
      picture_(static_cast<std::size_t>(width) * height * kRgbaBytes) {}

VideoDecoder::VideoDecoder(VideoDecoder&&) noexcept = default;
VideoDecoder& VideoDecoder::operator=(VideoDecoder&&) noexcept = default;
VideoDecoder::~VideoDecoder() = default;

Result<void> VideoDecoder::Decode(std::span<const std::byte> packet) {
    if (packet.empty()) {
        return {};
    }
    packet_.assign(packet.size() + AV_INPUT_BUFFER_PADDING_SIZE, 0);
    std::memcpy(packet_.data(), packet.data(), packet.size());
    AVPacket& input = *codec_->packet;
    input.data = packet_.data();
    input.size = static_cast<int>(packet.size());
    if (const int sent = avcodec_send_packet(codec_->context.get(), &input); sent < 0) {
        return std::unexpected(CodecError("can't decode a video frame", sent));
    }
    AVFrame& frame = *codec_->frame;
    const int received = avcodec_receive_frame(codec_->context.get(), &frame);
    if (received == AVERROR(EAGAIN)) {
        return {};
    }
    if (received < 0) {
        return std::unexpected(CodecError("can't decode a video frame", received));
    }
    if (frame.format != AV_PIX_FMT_YUV410P || std::cmp_less(frame.width, width_) ||
        std::cmp_less(frame.height, height_)) {
        av_frame_unref(&frame);
        return Fail(ErrorCode::InvalidData, "video frame has an unexpected format");
    }
    constexpr std::size_t kChromaShift = 2;
    const std::size_t chromaRows = (height_ + 3) >> kChromaShift;
    YuvToRgba({.y = {.samples = PlaneOf(frame, 0, height_),
                     .stride = static_cast<std::size_t>(frame.linesize[0])},
               .u = {.samples = PlaneOf(frame, 1, chromaRows),
                     .stride = static_cast<std::size_t>(frame.linesize[1])},
               .v = {.samples = PlaneOf(frame, 2, chromaRows),
                     .stride = static_cast<std::size_t>(frame.linesize[2])},
               .width = width_,
               .height = height_,
               .chromaShift = kChromaShift},
              picture_);
    av_frame_unref(&frame);
    return {};
}

}
