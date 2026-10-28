#include "platform/png_file.hpp"

#include <SDL3/SDL_error.h>
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

}

Result<void> SavePng(const std::filesystem::path& path, std::uint32_t widthPx,
                     std::uint32_t heightPx, std::span<const std::byte> rgba) {
    RR_ASSERT(rgba.size() == std::size_t{widthPx} * heightPx * kBytesPerPixel);
    const std::unique_ptr<SDL_Surface, SurfaceDeleter> surface(SDL_CreateSurfaceFrom(
        static_cast<int>(widthPx), static_cast<int>(heightPx), SDL_PIXELFORMAT_RGBA32,
        const_cast<std::byte*>(rgba.data()), static_cast<int>(widthPx * kBytesPerPixel)));
    if (!surface || !SDL_SavePNG(surface.get(), Utf8FromPath(path).c_str())) {
        return Fail(ErrorCode::Io,
                    std::format("can't save {}: {}", Utf8FromPath(path), SDL_GetError()));
    }
    return {};
}

}
