#include "render/fx_textures.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace rr::render {
namespace {

constexpr std::uint32_t kAtlasCells = 2;
constexpr std::uint32_t kAtlasPx = kSmokeCellPx * kAtlasCells;
constexpr std::uint32_t kOctaves = 5;
constexpr float kBaseFrequency = 4.0F;
constexpr std::size_t kBlobs = 7;
constexpr float kBlobSpread = 0.42F;
constexpr float kBlobMinRadius = 0.28F;
constexpr float kBlobMaxRadius = 0.48F;
constexpr float kCellFadeStart = 0.75F;
constexpr float kCellFadeEnd = 0.98F;
constexpr std::size_t kChannels = 4;
constexpr std::uint32_t kSparkWidthPx = 16;
constexpr std::uint32_t kSparkHeightPx = 64;
constexpr float kSparkSharpness = 6.0F;
constexpr std::uint32_t kGlowPx = 64;

float Hash(int x, int y, std::uint32_t seed) {
    std::uint32_t h = (static_cast<std::uint32_t>(x) * 0x8DA6B343U) ^
                      (static_cast<std::uint32_t>(y) * 0xD8163841U) ^ (seed * 0xCB1AB31FU);
    h ^= h >> 13U;
    h *= 0x5BD1E995U;
    h ^= h >> 15U;
    return static_cast<float>(h & 0xFFFFU) / 65535.0F;
}

float Smooth(float t) {
    return t * t * (3.0F - (2.0F * t));
}

float ValueNoise(float x, float y, std::uint32_t seed) {
    const float fx = std::floor(x);
    const float fy = std::floor(y);
    const int ix = static_cast<int>(fx);
    const int iy = static_cast<int>(fy);
    const float tx = Smooth(x - fx);
    const float ty = Smooth(y - fy);
    const float top = std::lerp(Hash(ix, iy, seed), Hash(ix + 1, iy, seed), tx);
    const float bottom = std::lerp(Hash(ix, iy + 1, seed), Hash(ix + 1, iy + 1, seed), tx);
    return std::lerp(top, bottom, ty);
}

float Fractal(float x, float y, std::uint32_t seed) {
    float sum = 0.0F;
    float weight = 0.5F;
    float frequency = kBaseFrequency;
    for (std::uint32_t octave = 0; octave < kOctaves; ++octave) {
        sum += weight * ValueNoise(x * frequency, y * frequency, seed + octave);
        weight *= 0.5F;
        frequency *= 2.0F;
    }
    return sum / (1.0F - (weight * 2.0F));
}

float SmoothStep(float edge0, float edge1, float x) {
    return Smooth(std::clamp((x - edge0) / (edge1 - edge0), 0.0F, 1.0F));
}

std::byte ToByte(float value) {
    return static_cast<std::byte>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

void Fill(assets::ImageAsset& image, std::size_t pixel, float light, float alpha) {
    const std::size_t at = pixel * kChannels;
    image.rgba[at] = ToByte(light);
    image.rgba[at + 1] = ToByte(light);
    image.rgba[at + 2] = ToByte(light);
    image.rgba[at + 3] = ToByte(alpha);
}

struct Blob {
    float x = 0.0F;
    float y = 0.0F;
    float radius = 0.0F;
};

std::array<Blob, kBlobs> BlobsOf(std::uint32_t seed) {
    std::array<Blob, kBlobs> blobs{};
    blobs[0] = {.radius = kBlobMaxRadius};
    for (std::size_t i = 1; i < kBlobs; ++i) {
        const auto index = static_cast<int>(i);
        const float angle = Hash(index, 0, seed) * 2.0F * std::numbers::pi_v<float>;
        const float distance = Hash(index, 1, seed) * kBlobSpread;
        blobs.at(i) = {.x = std::cos(angle) * distance,
                       .y = std::sin(angle) * distance,
                       .radius = std::lerp(kBlobMinRadius, kBlobMaxRadius, Hash(index, 2, seed))};
    }
    return blobs;
}

float HeightAt(const std::array<Blob, kBlobs>& blobs, float x, float y) {
    float height = 0.0F;
    for (const Blob& blob : blobs) {
        const float dx = x - blob.x;
        const float dy = y - blob.y;
        const float inside = (blob.radius * blob.radius) - (dx * dx) - (dy * dy);
        if (inside > 0.0F) {
            height = std::max(height, std::sqrt(inside) / blob.radius);
        }
    }
    return height;
}

void DrawPuff(assets::ImageAsset& atlas, std::size_t variant) {
    const auto cell = static_cast<std::uint32_t>(variant);
    const std::uint32_t left = (cell % kAtlasCells) * kSmokeCellPx;
    const std::uint32_t top = (cell / kAtlasCells) * kSmokeCellPx;
    const std::uint32_t seed = (cell * 101U) + 17U;
    const std::array<Blob, kBlobs> blobs = BlobsOf(seed);
    for (std::uint32_t y = 0; y < kSmokeCellPx; ++y) {
        for (std::uint32_t x = 0; x < kSmokeCellPx; ++x) {
            const float u = ((static_cast<float>(x) + 0.5F) / static_cast<float>(kSmokeCellPx));
            const float v = ((static_cast<float>(y) + 0.5F) / static_cast<float>(kSmokeCellPx));
            const float dx = (u * 2.0F) - 1.0F;
            const float dy = (v * 2.0F) - 1.0F;
            const float detail = Fractal(u, v, seed);
            const float wisp = Fractal(u + 7.3F, v + 3.1F, seed + 50U);
            const float height = HeightAt(blobs, dx, dy) + (0.35F * (detail - 0.5F));
            const float window =
                1.0F - SmoothStep(kCellFadeStart, kCellFadeEnd, std::sqrt((dx * dx) + (dy * dy)));
            const float density =
                SmoothStep(0.02F, 0.5F, height) * (0.65F + (0.35F * wisp)) * window;
            const float light =
                0.45F + (0.55F * std::clamp(height, 0.0F, 1.0F)) + (0.12F * (detail - 0.5F));
            Fill(atlas, (static_cast<std::size_t>(top + y) * kAtlasPx) + left + x, light, density);
        }
    }
}

}

assets::ImageAsset MakeSmokeAtlas() {
    assets::ImageAsset atlas{
        .widthPx = kAtlasPx,
        .heightPx = kAtlasPx,
        .rgba = std::vector<std::byte>(std::size_t{kAtlasPx} * kAtlasPx * kChannels)};
    for (std::size_t variant = 0; variant < kSmokeVariants; ++variant) {
        DrawPuff(atlas, variant);
    }
    return atlas;
}

assets::ImageAsset MakeSparkTexture() {
    assets::ImageAsset spark{
        .widthPx = kSparkWidthPx,
        .heightPx = kSparkHeightPx,
        .rgba = std::vector<std::byte>(std::size_t{kSparkWidthPx} * kSparkHeightPx * kChannels)};
    for (std::uint32_t y = 0; y < kSparkHeightPx; ++y) {
        const float along = (static_cast<float>(y) + 0.5F) / static_cast<float>(kSparkHeightPx);
        const float head = 1.0F - SmoothStep(0.0F, 1.0F, along);
        const float tip = SmoothStep(0.0F, 0.08F, along);
        for (std::uint32_t x = 0; x < kSparkWidthPx; ++x) {
            const float across =
                ((static_cast<float>(x) + 0.5F) / static_cast<float>(kSparkWidthPx) * 2.0F) - 1.0F;
            const float core = std::exp(-(across * across) * kSparkSharpness);
            Fill(spark, (std::size_t{y} * kSparkWidthPx) + x, 1.0F, core * head * tip);
        }
    }
    return spark;
}

assets::ImageAsset MakeGlowTexture() {
    assets::ImageAsset glow{
        .widthPx = kGlowPx,
        .heightPx = kGlowPx,
        .rgba = std::vector<std::byte>(std::size_t{kGlowPx} * kGlowPx * kChannels)};
    for (std::uint32_t y = 0; y < kGlowPx; ++y) {
        for (std::uint32_t x = 0; x < kGlowPx; ++x) {
            const float dx =
                ((static_cast<float>(x) + 0.5F) / static_cast<float>(kGlowPx) * 2.0F) - 1.0F;
            const float dy =
                ((static_cast<float>(y) + 0.5F) / static_cast<float>(kGlowPx) * 2.0F) - 1.0F;
            const float falloff = std::max(0.0F, 1.0F - (dx * dx) - (dy * dy));
            Fill(glow, (std::size_t{y} * kGlowPx) + x, 1.0F, falloff * falloff * falloff);
        }
    }
    return glow;
}

Rect SmokeCell(std::size_t variant) {
    constexpr float kCell = 1.0F / static_cast<float>(kAtlasCells);
    const std::size_t cell = variant % kSmokeVariants;
    const std::size_t column = cell % kAtlasCells;
    const std::size_t row = cell / kAtlasCells;
    return {.x = static_cast<float>(column) * kCell,
            .y = static_cast<float>(row) * kCell,
            .width = kCell,
            .height = kCell};
}

}
