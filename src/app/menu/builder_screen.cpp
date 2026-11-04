#include "app/menu/builder_screen.hpp"

#include <string>
#include <utility>

#include "app/menu/builder_settings.hpp"
#include "app/menu/car_build_screen.hpp"
#include "app/menu/workshop/workshop_screen.hpp"

namespace rr::app {

BuildMode BuildModeOf(const settings::Registry& settings) {
    const render::Color fallback =
        ParseHexColour(kBuilderColour.defaultValue).value_or(render::Color{});
    return {.controls =
                settings.Get(kEnhancedBuilder) ? BuildControls::Workshop : BuildControls::Original,
            .offersExtraPieces = settings.Get(kExtraPiecesSetting),
            .backdrop = ParseHexColour(settings.Get(kBuilderColour)).value_or(fallback),
            .background = std::string(settings.Get(kBuilderBackground))};
}

Result<std::unique_ptr<BuilderScreen>> OpenBuilderScreen(render::Renderer& renderer,
                                                         const assets::Vfs& vfs,
                                                         const BuildServices& services,
                                                         sim::BuiltCar car,
                                                         std::vector<std::size_t> sets) {
    const auto opened = [](auto screen) {
        return std::unique_ptr<BuilderScreen>(std::move(screen));
    };
    if (services.mode.controls == BuildControls::Workshop) {
        return WorkshopScreen::Load(renderer, services, std::move(car), std::move(sets))
            .transform(opened);
    }
    return CarBuildScreen::Load(renderer, vfs, services, std::move(car), std::move(sets))
        .transform(opened);
}

}
