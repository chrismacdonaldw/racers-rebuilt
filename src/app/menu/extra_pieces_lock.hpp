#pragma once

#include <cstdint>
#include <vector>

#include "app/menu/build_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "assets/car_data.hpp"
#include "render/render_view.hpp"

namespace rr::app {

inline constexpr std::int32_t kLockedSound = kBuildRefusedSound;

/// A car with extra pieces can't race or be saved while their setting is off.
[[nodiscard]] bool IsLockedCar(const assets::CarData& car, bool allowsExtraPieces);

/// Says why the car is locked, along the bottom of `area`.
void DrawLockNotice(const MenuText& text, const render::Rect& area,
                    std::vector<render::SpriteDraw>& out);

}
