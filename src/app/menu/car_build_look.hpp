#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "app/menu/car_edit.hpp"
#include "app/menu/carousel_layout.hpp"
#include "app/menu/carousel_view.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/menu/stand_scene.hpp"
#include "assets/menu_layout.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

inline constexpr std::string_view kCarBuildScreen = "CARBUILD";

inline constexpr std::size_t kBuildButtons = 6;

inline constexpr std::size_t kBuildPads = 2;

struct StateButton {
    std::array<render::TextureId, assets::kControlStates> images{};
    render::Rect bounds;
};

struct PadSpot {
    std::int32_t id = 0;
    render::Rect area;
};

/// `highlight` covers the whole pad; each spot shows its own part of it.
struct PadLook {
    std::vector<PadSpot> spots;
    UiImage highlight;
    MenuPoint origin;
};

struct CarBuildLook {
    MenuPicture backdrop;
    SelectorArrows arrows;
    SelectorPlace sets;
    render::Rect setBox;
    std::array<UiImage, kPartSetCount> setImages;
    SelectorPlace pieces;
    CarouselLayout carousel;
    CarouselColors itemColors{};
    Milliseconds scroll{0};
    assets::CarouselSounds pieceSounds{};
    float spinPerMs = 0.0F;
    Milliseconds setSlide{0};
    assets::CarouselSounds setSounds{};
    std::array<StateButton, kBuildButtons> buttons;
    std::array<PadLook, kBuildPads> pads;
    StandFraming garage;
    render::Camera camera;
};

[[nodiscard]] Result<CarBuildLook> LoadCarBuildLook(render::Renderer& renderer,
                                                    const assets::Vfs& vfs, const MenuFiles& files);

}
