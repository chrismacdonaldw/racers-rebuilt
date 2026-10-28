#pragma once

#include <SDL3/SDL_error.h>

#include <format>
#include <string_view>

#include "core/result.hpp"

namespace rr::render {

[[nodiscard]] inline std::unexpected<Error> GpuFailure(std::string_view what) {
    return Fail(ErrorCode::Platform, std::format("{}: {}", what, SDL_GetError()));
}

}
