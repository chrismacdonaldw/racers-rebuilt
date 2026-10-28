#pragma once

#include <memory>
#include <string>

#include "core/result.hpp"

struct SDL_Window;

namespace rr::platform {

/// Initial size and title of a window.
struct WindowConfig {
    std::string title;
    int widthPx = 0;
    int heightPx = 0;
};

/// Resizable top-level window. Requires a live `SdlContext`.
class Window {
public:
    /// Creates and shows a window. Sizes are in logical pixels and must be positive.
    [[nodiscard]] static Result<Window> Create(const WindowConfig& config);

private:
    struct Deleter {
        void operator()(SDL_Window* window) const;
    };

    explicit Window(SDL_Window* window);

    std::unique_ptr<SDL_Window, Deleter> window_;
};

}
