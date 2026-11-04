#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/track_entity_draws.hpp"
#include "app/track_model.hpp"
#include "assets/race_catalog.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

/// The boss of the last circuit open in story order from the first: the next champion to beat.
[[nodiscard]] std::string MenuChampionCode(const assets::RaceCatalog& races,
                                           const std::set<std::string>& openCircuits);

/// A progress file or race list that won't load gives the first champion, with a warning.
[[nodiscard]] std::string SavedMenuChampion(const assets::Vfs& vfs,
                                            const std::filesystem::path& progressFile);

class MenuChampion {
public:
    /// `files` are the main menu's, whose `platform` element places the champion.
    [[nodiscard]] static Result<std::unique_ptr<MenuChampion>> Load(render::Renderer& renderer,
                                                                    const assets::Vfs& vfs,
                                                                    const MenuFiles& files,
                                                                    std::string_view champion);

    MenuChampion(const MenuChampion&) = delete;
    MenuChampion& operator=(const MenuChampion&) = delete;
    MenuChampion(MenuChampion&&) = delete;
    MenuChampion& operator=(MenuChampion&&) = delete;
    ~MenuChampion() = default;

    /// Returns whether the glass tap sounds this tick.
    [[nodiscard]] bool Tick(Milliseconds elapsed);

    [[nodiscard]] std::span<const sim::EntityClock> Clocks() const { return entities_.Clocks(); }

    [[nodiscard]] const render::Rect& Bounds() const { return bounds_; }

    [[nodiscard]] render::RenderView Render(std::span<const sim::EntityClock> clocks,
                                            std::span<const MenuPicture> pictures);

private:
    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Parts {
        sim::TrackEntitySources sources;
        assets::NameMap<JointedModel> models;
        render::Camera camera;
        render::Rect bounds;
    };

    explicit MenuChampion(Parts parts);

    sim::TrackEntitySources sources_;
    assets::NameMap<JointedModel> models_;
    render::Camera camera_;
    render::Rect bounds_;
    sim::TrackEntities entities_;
    TrackEntityDraws jointed_;
    Milliseconds untilTap_;
    bool hasTapped_ = false;
    std::vector<render::MeshDraw> draws_;
    std::vector<render::QuadDraw> quads_;
    std::array<render::DirectionalLight, 1> light_{};
};

}
