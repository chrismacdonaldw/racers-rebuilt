#pragma once

#include <array>
#include <string_view>

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

struct StateButton {
    std::array<render::TextureId, assets::kControlStates> images{};
    render::Rect bounds;
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
    std::array<StateButton, kBuildButtons> buttons;
    StandFraming garage;
    render::Camera camera;
};

[[nodiscard]] Result<CarBuildLook> LoadCarBuildLook(render::Renderer& renderer,
                                                    const assets::Vfs& vfs, const MenuFiles& files);

}
