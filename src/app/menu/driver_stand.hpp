#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "app/menu/driver_kit.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/part_carousel.hpp"
#include "app/menu/stand_scene.hpp"
#include "app/track_entity_draws.hpp"
#include "app/track_model.hpp"
#include "assets/skeleton_database.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/race/random.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::app {

/// The minifig in `CB_SET`, idling, reacting to changes and leaving.
class DriverStand {
public:
    [[nodiscard]] static Result<std::unique_ptr<DriverStand>> Load(render::Renderer& renderer,
                                                                   const assets::Vfs& vfs,
                                                                   assets::TrackLoader& loader,
                                                                   const MenuFiles& files,
                                                                   std::string_view screen);

    DriverStand(const DriverStand&) = delete;
    DriverStand& operator=(const DriverStand&) = delete;
    DriverStand(DriverStand&&) = delete;
    DriverStand& operator=(DriverStand&&) = delete;
    ~DriverStand() = default;

    /// Fails, leaving the stand as it was, when the animations don't fit the minifig's skeleton.
    [[nodiscard]] Result<void> Show(KitMinifig minifig);

    /// Queues the slot's reaction for when the playing part ends.
    void React(DriverSlot slot);

    void Leave();

    /// Only while a leaving part plays, so a stand with nothing shown leaves at once.
    [[nodiscard]] bool IsLeaving() const;

    void Tick(Milliseconds elapsed);

    [[nodiscard]] std::span<const sim::EntityClock> Clocks() const { return entities_.Clocks(); }
    [[nodiscard]] const StandFraming& Framing() const { return framing_; }

    [[nodiscard]] render::RenderView Render(std::span<const sim::EntityClock> clocks);

private:
    struct AddedSkeleton {
        const assets::SkeletonAsset* skeleton = nullptr;
        std::size_t index = 0;
    };

    DriverStand(std::unique_ptr<StandScene> scene, TrackModel model, const StandFraming& framing);

    [[nodiscard]] Result<std::size_t> SkeletonOf(const assets::SkeletonAsset& skeleton);
    void PlayNext();

    std::unique_ptr<StandScene> scene_;
    TrackModel model_;
    StandFraming framing_;
    sim::TrackEntities entities_;
    TrackEntityDraws jointed_;
    std::vector<AddedSkeleton> skeletons_;
    std::optional<std::size_t> pending_;
    sim::Random random_;
    bool isShown_ = false;
    bool isLeaving_ = false;
    std::vector<render::MeshDraw> draws_;
    std::array<render::DirectionalLight, 1> light_{};
};

}
