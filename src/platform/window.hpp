#pragma once

#include <memory>
#include <string>

#include "core/result.hpp"

struct SDL_Window;

namespace rr::platform {

struct WindowConfig {
    std::string title;
    int widthPx = 0;
    int heightPx = 0;
    bool isFullscreen = false;
};

/// Requires a live `SdlContext`.
class Window {
public:
    /// Sizes are in logical pixels and must be positive.
    [[nodiscard]] static Result<Window> Create(const WindowConfig& config);

    [[nodiscard]] SDL_Window* Handle() const { return window_.get(); }

    /// Fullscreen is borderless at the desktop's size.
    Result<void> SetFullscreen(bool isFullscreen);

private:
    struct Deleter {
        void operator()(SDL_Window* window) const;
    };

    explicit Window(SDL_Window* window);

    std::unique_ptr<SDL_Window, Deleter> window_;
};

}
