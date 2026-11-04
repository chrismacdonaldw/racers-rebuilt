#include "app/menu_pointer.hpp"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <cstdint>
#include <string_view>

#include "app/menu/menu_files.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kCursorFiles = "MAINMENU";
constexpr std::string_view kCursorImage = "arrow";

sim::PointerButton ButtonOf(std::uint8_t button) {
    switch (button) {
        case SDL_BUTTON_LEFT:
            return sim::PointerButton::Left;
        case SDL_BUTTON_RIGHT:
            return sim::PointerButton::Right;
        case SDL_BUTTON_MIDDLE:
            return sim::PointerButton::Middle;
        default:
            return sim::PointerButton::Other;
    }
}

}

CanvasPoint ToCanvas(float xPx, float yPx, const render::PixelRect& area) {
    if (area.widthPx == 0 || area.heightPx == 0) {
        return {.x = -1.0F, .y = -1.0F};
    }
    return {.x = (xPx - static_cast<float>(area.xPx)) * render::kUiWidth /
                 static_cast<float>(area.widthPx),
            .y = (yPx - static_cast<float>(area.yPx)) * render::kUiHeight /
                 static_cast<float>(area.heightPx)};
}

bool IsOnCanvas(const CanvasPoint& point) {
    return point.x >= 0.0F && point.y >= 0.0F && point.x < render::kUiWidth &&
           point.y < render::kUiHeight;
}

std::optional<UiImage> LoadMenuCursor(render::Renderer& renderer, const assets::Vfs& vfs) {
    Result<MenuFiles> files = ReadMenuFiles(vfs, kCursorFiles);
    const assets::ImageListEntry* entry = files ? files->FindImage(kCursorImage) : nullptr;
    if (entry == nullptr) {
        log::Warn("the menus have no {} cursor", kCursorImage);
        return std::nullopt;
    }
    Result<UiImage> cursor = LoadUiImage(renderer, vfs, kMenuFolder, *entry);
    if (!cursor) {
        log::Warn("the menu cursor didn't load: {}", cursor.error().message);
        return std::nullopt;
    }
    return *cursor;
}

void MenuPointer::Handle(const SDL_Event& event, sim::InputLatch& latch) {
    switch (event.type) {
        case SDL_EVENT_MOUSE_MOTION:
            latch.AddPointerMotion(event.motion.xrel, event.motion.yrel);
            Move(event.motion.x, event.motion.y, latch);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            Move(event.button.x, event.button.y, latch);
            latch.PressPointer(ButtonOf(event.button.button),
                               (SDL_GetModState() & SDL_KMOD_SHIFT) != 0);
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            Move(event.button.x, event.button.y, latch);
            latch.ReleasePointer(ButtonOf(event.button.button));
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            latch.ScrollPointer(event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y
                                                                                : event.wheel.y);
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            if (at_) {
                latch.MovePointer(at_->x, at_->y, false);
            }
            at_.reset();
            break;
        default:
            break;
    }
}

void MenuPointer::Show(bool isShown, bool isHeld) {
    Hold(isShown && isHeld);
    const bool drawsCursor = isShown && !isHeld_ && cursor_ && at_ && IsOnCanvas(*at_);
    if (drawsCursor) {
        renderer_->SetPointer(
            render::SpriteDraw{.texture = cursor_->texture,
                               .bounds = {.x = at_->x,
                                          .y = at_->y,
                                          .width = static_cast<float>(cursor_->widthPx),
                                          .height = static_cast<float>(cursor_->heightPx)}});
    } else {
        renderer_->SetPointer(std::nullopt);
    }
    const bool hidesSystemCursor = drawsCursor || isHeld_;
    if (hidesSystemCursor != isHidingSystemCursor_) {
        isHidingSystemCursor_ = hidesSystemCursor;
        static_cast<void>(hidesSystemCursor ? SDL_HideCursor() : SDL_ShowCursor());
    }
}

void MenuPointer::Hold(bool isHeld) {
    if (isHeld == isHeld_) {
        return;
    }
    isHeld_ = isHeld;
    if (isHeld) {
        static_cast<void>(SDL_GetMouseState(&heldAtX_, &heldAtY_));
    }
    static_cast<void>(SDL_SetWindowRelativeMouseMode(window_, isHeld));
    if (!isHeld) {
        SDL_WarpMouseInWindow(window_, heldAtX_, heldAtY_);
    }
}

void MenuPointer::Move(float x, float y, sim::InputLatch& latch) {
    if (isHeld_) {
        return;
    }
    int widthPx = 0;
    int heightPx = 0;
    static_cast<void>(SDL_GetWindowSizeInPixels(window_, &widthPx, &heightPx));
    const float density = SDL_GetWindowPixelDensity(window_);
    const render::PixelRect area =
        render::FitUiArea(static_cast<std::uint32_t>(widthPx), static_cast<std::uint32_t>(heightPx),
                          renderer_->Fit());
    const CanvasPoint point = ToCanvas(x * density, y * density, area);
    at_ = point;
    latch.MovePointer(point.x, point.y, IsOnCanvas(point));
}

}
