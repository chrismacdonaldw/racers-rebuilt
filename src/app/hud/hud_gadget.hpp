#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "app/hud/minimap.hpp"
#include "app/ui_text/ui_image.hpp"
#include "render/render_view.hpp"

namespace rr::app {

enum class HudGadgetMode : std::uint8_t { None, FixedMap, Speedometer, RotatingMap };

/// The bottom right corner of the race HUD. A race starts on the fixed map.
class HudGadget {
public:
    explicit HudGadget(std::optional<TrackMap> map = std::nullopt) : map_(map) {}

    void Cycle();

    /// `forwardSpeed` is in units per ms.
    void Tick(float forwardSpeed);

    [[nodiscard]] HudGadgetMode Mode() const { return mode_; }

    /// `images` is `LEGOIMGS.IDB` in list order. Draws nothing while it's short.
    void Draw(const MapView& view, std::span<const UiImage> images,
              std::vector<render::SpriteDraw>& out) const;

private:
    std::optional<TrackMap> map_;
    HudGadgetMode mode_ = HudGadgetMode::FixedMap;
    float speed_ = 0.0F;
};

void DrawSpeedometer(float speed, const UiImage& dial, const UiImage& cap,
                     std::vector<render::SpriteDraw>& out);

}
