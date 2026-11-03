#pragma once

#include <SDL3/SDL_events.h>

#include <optional>

#include "app/ui_text/ui_image.hpp"
#include "assets/vfs.hpp"
#include "render/renderer.hpp"
#include "render/ui_layout.hpp"
#include "sim/input_latch.hpp"

struct SDL_Window;

namespace rr::app {

struct CanvasPoint {
    float x = 0.0F;
    float y = 0.0F;
};

/// `area` is where the UI canvas lies in the window, in the same pixels as `xPx` and `yPx`.
[[nodiscard]] CanvasPoint ToCanvas(float xPx, float yPx, const render::PixelRect& area);

[[nodiscard]] bool IsOnCanvas(const CanvasPoint& point);

/// Null when the menu images lack the cursor.
[[nodiscard]] std::optional<UiImage> LoadMenuCursor(render::Renderer& renderer,
                                                    const assets::Vfs& vfs);

class MenuPointer {
public:
    /// `window` and `renderer` must outlive it.
    MenuPointer(SDL_Window* window, render::Renderer& renderer, std::optional<UiImage> cursor)
        : window_(window), renderer_(&renderer), cursor_(cursor) {}

    void Handle(const SDL_Event& event, sim::InputLatch& latch);

    /// Call once a frame.
    void Show(bool isShown);

private:
    void Move(float x, float y, sim::InputLatch& latch);

    SDL_Window* window_;
    render::Renderer* renderer_;
    std::optional<UiImage> cursor_;
    std::optional<CanvasPoint> at_;
    bool isHidingSystemCursor_ = false;
};

}
