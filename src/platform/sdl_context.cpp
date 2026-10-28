#include "platform/sdl_context.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>

#include <format>
#include <utility>

namespace rr::platform {

Result<SdlContext> SdlContext::Create() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return Fail(ErrorCode::Platform, std::format("SDL failed to start: {}", SDL_GetError()));
    }
    return SdlContext();
}

SdlContext::SdlContext(SdlContext&& other) noexcept
    : isOwner_(std::exchange(other.isOwner_, false)) {}

SdlContext::~SdlContext() {
    if (isOwner_) {
        SDL_Quit();
    }
}

}
