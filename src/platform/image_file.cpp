#include "platform/image_file.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_surface.h>

#include <format>
#include <memory>

#include "core/assert.hpp"
#include "core/utf8_path.hpp"

namespace rr::platform {
namespace {

constexpr std::uint32_t kBytesPerPixel = 4;

struct SurfaceDeleter {
    void operator()(SDL_Surface* surface) const { SDL_DestroySurface(surface); }
};

struct StreamCloser {
    void operator()(SDL_IOStream* stream) const { SDL_CloseIO(stream); }
};

using Surface = std::unique_ptr<SDL_Surface, SurfaceDeleter>;
using SaveSurface = bool (*)(SDL_Surface* surface, const char* file);

Surface WrapPixels(std::uint32_t widthPx, std::uint32_t heightPx, std::span<const std::byte> rgba) {
    RR_ASSERT(rgba.size() == std::size_t{widthPx} * heightPx * kBytesPerPixel);
    return Surface(SDL_CreateSurfaceFrom(
        static_cast<int>(widthPx), static_cast<int>(heightPx), SDL_PIXELFORMAT_RGBA32,
        const_cast<std::byte*>(rgba.data()), static_cast<int>(widthPx * kBytesPerPixel)));
}

Result<void> Save(const std::filesystem::path& path, std::uint32_t widthPx, std::uint32_t heightPx,
                  std::span<const std::byte> rgba, SaveSurface save) {
    const Surface surface = WrapPixels(widthPx, heightPx, rgba);
    if (!surface || !save(surface.get(), Utf8FromPath(path).c_str())) {
        return Fail(ErrorCode::Io,
                    std::format("can't save {}: {}", Utf8FromPath(path), SDL_GetError()));
    }
    return {};
}

}

Result<void> SavePng(const std::filesystem::path& path, std::uint32_t widthPx,
                     std::uint32_t heightPx, std::span<const std::byte> rgba) {
    return Save(path, widthPx, heightPx, rgba, SDL_SavePNG);
}

Result<void> SaveBmp(const std::filesystem::path& path, std::uint32_t widthPx,
                     std::uint32_t heightPx, std::span<const std::byte> rgba) {
    return Save(path, widthPx, heightPx, rgba, SDL_SaveBMP);
}

Result<std::vector<std::byte>> EncodePng(std::uint32_t widthPx, std::uint32_t heightPx,
                                         std::span<const std::byte> rgba) {
    const Surface surface = WrapPixels(widthPx, heightPx, rgba);
    const std::unique_ptr<SDL_IOStream, StreamCloser> stream(SDL_IOFromDynamicMem());
    if (!surface || !stream || !SDL_SavePNG_IO(surface.get(), stream.get(), false)) {
        return Fail(ErrorCode::Io, std::format("can't encode a PNG: {}", SDL_GetError()));
    }
    const Sint64 size = SDL_TellIO(stream.get());
    const auto* data = static_cast<const std::byte*>(SDL_GetPointerProperty(
        SDL_GetIOProperties(stream.get()), SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, nullptr));
    if (size <= 0 || data == nullptr) {
        return Fail(ErrorCode::Io, "can't encode a PNG: no bytes written");
    }
    return std::vector<std::byte>(data, data + size);
}

}
