#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "assets/avi_file.hpp"
#include "core/result.hpp"

namespace rr::video {

class VideoDecoder {
public:
    /// Fails with `NotFound` for any codec but Indeo 5.
    [[nodiscard]] static Result<VideoDecoder> Create(const assets::AviVideo& video);

    VideoDecoder(VideoDecoder&&) noexcept;
    VideoDecoder& operator=(VideoDecoder&&) noexcept;
    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;
    ~VideoDecoder();

    /// An empty `packet` keeps the last picture.
    [[nodiscard]] Result<void> Decode(std::span<const std::byte> packet);

    /// Rows of RGBA8, top first.
    [[nodiscard]] std::span<const std::uint8_t> Picture() const { return picture_; }

private:
    struct Codec;

    VideoDecoder(std::unique_ptr<Codec> codec, std::uint32_t width, std::uint32_t height);

    std::unique_ptr<Codec> codec_;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::vector<std::uint8_t> packet_;
    std::vector<std::uint8_t> picture_;
};

}
