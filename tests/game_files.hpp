#pragma once

#include <SDL3/SDL_stdinc.h>

#include <filesystem>
#include <optional>

#include "core/utf8_path.hpp"

namespace rr::test {

[[nodiscard]] inline std::optional<std::filesystem::path> GameFolder() {
    const char* path = SDL_getenv("RR_GAME_PATH");
    if (path == nullptr || *path == '\0') {
        return std::nullopt;
    }
    return PathFromUtf8(path);
}

}
