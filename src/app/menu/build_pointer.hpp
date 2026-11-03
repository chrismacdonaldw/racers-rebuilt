#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "app/menu/car_build_look.hpp"
#include "app/menu/car_build_menu.hpp"
#include "app/menu/pointer_buttons.hpp"
#include "core/duration.hpp"
#include "render/render_view.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

struct PadPress {
    BuildPad pad = BuildPad::Move;
    std::size_t spot = 0;
};

struct BuildPointing {
    sim::InputFrame input;
    BuildRequest pad;
};

[[nodiscard]] BuildRequest PadRequest(BuildPad pad, std::int32_t spot);

/// A held pad fires again as a held key does.
class BuildPointer {
public:
    /// Hovering moves `menu`'s focus; a click is folded into the returned input.
    [[nodiscard]] BuildPointing Tick(const sim::InputFrame& input, const CarBuildLook& look,
                                     CarBuildMenu& menu);

    void DrawPressed(const CarBuildLook& look, std::vector<render::SpriteDraw>& sprites) const;

private:
    [[nodiscard]] BuildRequest Press(const sim::MenuPointer& pointer, const CarBuildLook& look);
    [[nodiscard]] BuildRequest Fired(const CarBuildLook& look) const;

    PointerButtons buttons_;
    std::vector<PointerTarget> targets_;
    std::optional<PadPress> press_;
    Milliseconds repeatIn_{0};
};

}
