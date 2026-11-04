#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "app/hud/hud_text.hpp"
#include "app/ui_text/ui_font.hpp"
#include "assets/loading_screen.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

struct LoadingScreenParts {
    render::TextureId background{};
    render::TextureId tick{};
    std::vector<std::array<float, 2>> ticks;
    std::u16string trackName;
};

/// Ticks are in the file's order, and `progress` runs from 0 to 1.
void LayoutLoadingScreen(const LoadingScreenParts& parts, const HudFonts& fonts, float progress,
                         std::vector<render::SpriteDraw>& out);

/// A track's picture with its name and a row of progress ticks, drawn while the race loads.
class RaceLoadingScreen {
public:
    [[nodiscard]] static Result<std::unique_ptr<RaceLoadingScreen>> Load(render::Renderer& renderer,
                                                                         const assets::Vfs& vfs,
                                                                         std::string_view track,
                                                                         std::string_view language,
                                                                         TextFixes fixes);

    /// Draws and presents a frame at once.
    void Show(float progress);

    [[nodiscard]] UiFonts TakeFonts() &&;

private:
    RaceLoadingScreen(render::Renderer& renderer, UiFonts fonts, LoadingScreenParts parts);

    render::Renderer* renderer_;
    UiFonts fonts_;
    HudFonts hudFonts_;
    LoadingScreenParts parts_;
    std::vector<render::SpriteDraw> sprites_;
};

/// Shows the screen at no progress. Null without its files.
[[nodiscard]] std::unique_ptr<RaceLoadingScreen> OpenLoadingScreen(render::Renderer& renderer,
                                                                   const assets::Vfs& vfs,
                                                                   std::string_view track,
                                                                   std::string_view language,
                                                                   TextFixes fixes);

}
