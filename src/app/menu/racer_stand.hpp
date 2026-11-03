#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/menu_files.hpp"
#include "app/menu/stand_racer.hpp"
#include "app/menu/stand_scene.hpp"
#include "app/track_entity_draws.hpp"
#include "app/track_model.hpp"
#include "assets/car_data.hpp"
#include "assets/driver_look.hpp"
#include "assets/loader_pool.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "settings/racers.hpp"
#include "sim/race/random.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::app {

/// The garage's and racer pick's `showcase`: a racer's minifig beside its car on a turntable.
class RacerStand {
public:
    /// `files` are the screen's, whose `showcase` element places the stand.
    [[nodiscard]] static Result<std::unique_ptr<RacerStand>> Load(render::Renderer& renderer,
                                                                  const assets::Vfs& vfs,
                                                                  const MenuFiles& files,
                                                                  std::string_view screen);

    RacerStand(const RacerStand&) = delete;
    RacerStand& operator=(const RacerStand&) = delete;
    RacerStand(RacerStand&&) = delete;
    RacerStand& operator=(RacerStand&&) = delete;
    ~RacerStand() = default;

    /// Builds each look and car once. One that fails to build empties the stand, with a warning.
    void Show(const settings::Racer& racer);

    void Clear();

    /// Plays one of the pick reactions, then holds its last frame instead of idling.
    void Cheer();

    [[nodiscard]] bool IsCheering() const;

    void Tick(Milliseconds elapsed);

    [[nodiscard]] std::span<const sim::EntityClock> Clocks() const { return entities_.Clocks(); }

    [[nodiscard]] const render::Rect& Bounds() const { return framing_.interior; }

    /// Appends the frame to `sprites`, which the stand leaves for the caller to fill and point at.
    [[nodiscard]] render::RenderView Render(std::span<const sim::EntityClock> clocks,
                                            std::vector<render::SpriteDraw>& sprites);

private:
    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Loading {
        std::unique_ptr<assets::LoaderPool> pool;
        std::unique_ptr<assets::TrackLoader> loader;
    };

    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Built {
        assets::DriverLook look;
        std::string chassis;
        assets::CarData car{};
        std::optional<StandRacer> parts;
        std::string model;
        std::size_t skeleton = 0;
    };

    RacerStand(const assets::Vfs& vfs, render::Renderer& renderer, Loading loading,
               std::unique_ptr<StandScene> scene, TrackModel model, const StandFraming& framing);

    [[nodiscard]] const Built& Build(const settings::Racer& racer);
    [[nodiscard]] Result<StandRacer> BuildParts(const settings::Racer& racer) const;
    void PlayIdle();
    void AppendCar(std::span<const sim::EntityClock> clocks, std::size_t turntable,
                   const JointedModel& car);

    const assets::Vfs* vfs_;
    render::Renderer* renderer_;
    Loading loading_;
    std::unique_ptr<StandScene> scene_;
    TrackModel model_;
    StandFraming framing_;
    sim::TrackEntities entities_;
    TrackEntityDraws jointed_;
    std::vector<Built> built_;
    std::optional<std::size_t> shown_;
    sim::Random random_;
    bool isCheering_ = false;
    std::vector<render::MeshDraw> draws_;
    std::array<render::DirectionalLight, 1> light_{};
};

}
