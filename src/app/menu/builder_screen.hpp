#pragma once

#include <SDL3/SDL_keycode.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "app/menu/car_stand.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/scene.hpp"
#include "assets/piece_catalog.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "settings/registry.hpp"
#include "sim/car_build/built_car.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

enum class BuildOutcome : std::uint8_t { None, CarChanged, TooManyPieces, Done };

enum class BuildControls : std::uint8_t { Original, Palette, Workshop };

struct BuildMode {
    BuildControls controls = BuildControls::Original;
    bool offersExtraPieces = false;
    render::Color backdrop;
    /// A race track's folder, or `kColourBackground`.
    std::string background;
};

struct BuildServices {
    const assets::Vfs* vfs = nullptr;
    CarStand* stand = nullptr;
    assets::PieceCatalog* catalog = nullptr;
    const MenuText* text = nullptr;
    MenuSounds sounds;
    BuildMode mode;
};

class BuilderScreen {
public:
    BuilderScreen() = default;
    BuilderScreen(const BuilderScreen&) = delete;
    BuilderScreen& operator=(const BuilderScreen&) = delete;
    BuilderScreen(BuilderScreen&&) = delete;
    BuilderScreen& operator=(BuilderScreen&&) = delete;
    virtual ~BuilderScreen() = default;

    virtual void HandleKey(SDL_Keycode key, SDL_Keymod mod) = 0;

    /// False when `mode` needs the other builder, which the caller then opens on `Car()`.
    [[nodiscard]] virtual bool UseControls(BuildMode mode) = 0;

    [[nodiscard]] virtual BuildOutcome Tick(const sim::InputFrame& input) = 0;
    [[nodiscard]] virtual bool HoldsPointer() const = 0;
    [[nodiscard]] virtual const sim::BuiltCar& Car() const = 0;
    [[nodiscard]] virtual std::vector<SettingChange> TakeSettingChanges() { return {}; }

    /// Adds at least one view.
    virtual void Render(std::vector<render::RenderView>& views,
                        std::vector<render::SpriteDraw>& sprites) = 0;
};

[[nodiscard]] BuildMode BuildModeOf(const settings::Registry& settings);

/// The workshop or the original builder, as `services.mode` asks. What `services` points to must
/// outlive it.
[[nodiscard]] Result<std::unique_ptr<BuilderScreen>> OpenBuilderScreen(
    render::Renderer& renderer, const assets::Vfs& vfs, const BuildServices& services,
    sim::BuiltCar car, std::vector<std::size_t> sets);

}
