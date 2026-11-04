#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/workshop/track_backdrop.hpp"
#include "app/ui_text/ui_font.hpp"
#include "render/render_view.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

inline constexpr std::size_t kBackgroundColours = 8;

/// `background` is a track's folder or `kColourBackground`, which alone sets `colour`.
struct BackgroundChoice {
    std::string background;
    std::optional<render::Color> colour;
};

struct BackgroundLayout {
    render::Rect panel;
    std::array<render::Rect, kBackgroundColours> colours{};
    std::vector<render::Rect> tracks;
};

/// Hangs under `button`, kept inside `scene`. Tracks fill two columns, row by row.
[[nodiscard]] BackgroundLayout LayOutBackgroundMenu(const render::Rect& button,
                                                    const render::Rect& scene, std::size_t tracks);

[[nodiscard]] render::Color BackgroundColour(std::size_t index);

/// The background tool's popover of colours and race tracks.
class BackgroundMenu {
public:
    explicit BackgroundMenu(std::vector<BackdropTrack> tracks);

    void Place(const render::Rect& button, const render::Rect& scene);
    void Toggle() { isOpen_ = !isOpen_; }
    void Close() { isOpen_ = false; }
    [[nodiscard]] bool IsOpen() const { return isOpen_; }
    [[nodiscard]] bool Covers(float x, float y) const;
    [[nodiscard]] const BackgroundLayout& Layout() const { return layout_; }
    [[nodiscard]] bool Offers(std::string_view folder) const;

    /// Closes once something is chosen.
    [[nodiscard]] std::optional<BackgroundChoice> Tick(const sim::MenuPointer& pointer);

    /// Outlines `background`, or the swatch matching `colour` when it is `kColourBackground`.
    void Render(const UiFont* font, std::string_view background, const render::Color& colour,
                std::vector<render::SpriteDraw>& sprites) const;

private:
    std::vector<BackdropTrack> tracks_;
    BackgroundLayout layout_;
    std::optional<render::Rect> hovered_;
    bool isOpen_ = false;
};

}
