#include "audio/decoded_file.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <utility>
#include <vector>

#include "core/utf8_path.hpp"

#include <miniaudio.h>

namespace rr::audio {
namespace {

constexpr ma_uint64 kFramesPerChunk = 16384;

class DecodedPcm final : public PcmStream {
public:
    DecodedPcm(PcmFormat format, std::vector<std::int16_t> samples)
        : format_(format), samples_(std::move(samples)) {}

    [[nodiscard]] PcmFormat Format() const override { return format_; }
    [[nodiscard]] std::uint64_t FrameCount() const override {
        return samples_.size() / format_.channelCount;
    }
    [[nodiscard]] std::uint64_t Cursor() const override { return cursor_; }

    std::uint64_t Read(std::span<std::int16_t> interleaved) noexcept override {
        const std::uint64_t frames = std::min<std::uint64_t>(
            interleaved.size() / format_.channelCount, FrameCount() - cursor_);
        const auto first =
            samples_.begin() + static_cast<std::ptrdiff_t>(cursor_ * format_.channelCount);
        std::copy_n(first, frames * format_.channelCount, interleaved.begin());
        cursor_ += frames;
        return frames;
    }
    void Seek(std::uint64_t frame) noexcept override { cursor_ = std::min(frame, FrameCount()); }

private:
    PcmFormat format_;
    std::vector<std::int16_t> samples_;
    std::uint64_t cursor_ = 0;
};

class Decoder {
public:
    Decoder() = default;
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder(Decoder&&) = delete;
    Decoder& operator=(Decoder&&) = delete;
    ~Decoder() {
        if (isOpen_) {
            ma_decoder_uninit(&decoder_);
        }
    }

    ma_result Open(const std::filesystem::path& path) {
        const ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 0, 0);
        const ma_result result = ma_decoder_init_file_w(path.c_str(), &config, &decoder_);
        isOpen_ = result == MA_SUCCESS;
        return result;
    }

    [[nodiscard]] PcmFormat Format() const {
        return {.channelCount = decoder_.outputChannels, .sampleRateHz = decoder_.outputSampleRate};
    }

    std::vector<std::int16_t> ReadAll() {
        const std::size_t channelCount = decoder_.outputChannels;
        std::vector<std::int16_t> samples;
        ma_uint64 read = kFramesPerChunk;
        while (read == kFramesPerChunk) {
            const std::size_t end = samples.size();
            samples.resize(end + (kFramesPerChunk * channelCount));
            ma_decoder_read_pcm_frames(&decoder_, &samples[end], kFramesPerChunk, &read);
            samples.resize(end + (read * channelCount));
        }
        return samples;
    }

private:
    ma_decoder decoder_{};
    bool isOpen_ = false;
};

}

Result<std::unique_ptr<PcmStream>> DecodeFile(const std::filesystem::path& path) {
    Decoder decoder;
    const ma_result opened = decoder.Open(path);
    if (opened != MA_SUCCESS) {
        return Fail(ErrorCode::InvalidData, std::format("can't decode {}: {}", Utf8FromPath(path),
                                                        ma_result_description(opened)));
    }
    std::vector<std::int16_t> samples = decoder.ReadAll();
    if (samples.empty()) {
        return Fail(ErrorCode::InvalidData, std::format("{} has no audio", Utf8FromPath(path)));
    }
    return std::make_unique<DecodedPcm>(decoder.Format(), std::move(samples));
}

}
