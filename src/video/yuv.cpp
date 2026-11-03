#include "video/yuv.hpp"

#include <algorithm>

namespace rr::video {
namespace {

constexpr int kLumaBlack = 16;
constexpr int kChromaZero = 128;
constexpr int kOpaque = 255;
constexpr int kShift = 8;
constexpr int kRound = 1 << (kShift - 1);
constexpr int kLumaScale = 298;
constexpr int kRedFromV = 409;
constexpr int kGreenFromU = 100;
constexpr int kGreenFromV = 208;
constexpr int kBlueFromU = 516;

std::uint8_t Clamp(int value) {
    return static_cast<std::uint8_t>(std::clamp(value >> kShift, 0, kOpaque));
}

}

void YuvToRgba(const YuvPicture& picture, std::span<std::uint8_t> rgba) {
    for (std::size_t row = 0; row < picture.height; ++row) {
        const std::size_t chromaRow = (row >> picture.chromaShift) * picture.u.stride;
        const std::size_t chromaRowV = (row >> picture.chromaShift) * picture.v.stride;
        for (std::size_t column = 0; column < picture.width; ++column) {
            const std::size_t chroma = column >> picture.chromaShift;
            const int c =
                kLumaScale * (picture.y.samples[(row * picture.y.stride) + column] - kLumaBlack);
            const int d = picture.u.samples[chromaRow + chroma] - kChromaZero;
            const int e = picture.v.samples[chromaRowV + chroma] - kChromaZero;
            const std::span<std::uint8_t> pixel =
                rgba.subspan(((row * picture.width) + column) * 4, 4);
            pixel[0] = Clamp(c + (kRedFromV * e) + kRound);
            pixel[1] = Clamp(c - (kGreenFromU * d) - (kGreenFromV * e) + kRound);
            pixel[2] = Clamp(c + (kBlueFromU * d) + kRound);
            pixel[3] = kOpaque;
        }
    }
}

}
