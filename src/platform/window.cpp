#include "platform/window.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>

#include <format>

#include "core/assert.hpp"

namespace rr::platform {

Result<Window> Window::Create(const WindowConfig& config) {
    RR_ASSERT(config.widthPx > 0 && config.heightPx > 0);

    SDL_Window* window = SDL_CreateWindow(config.title.c_str(), config.widthPx, config.heightPx,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        return Fail(ErrorCode::Platform, std::format("can't create window: {}", SDL_GetError()));
    }
    return Window(window);
}

Window::Window(SDL_Window* window) : window_(window) {}

void Window::Deleter::operator()(SDL_Window* window) const {
    SDL_DestroyWindow(window);
}

}
