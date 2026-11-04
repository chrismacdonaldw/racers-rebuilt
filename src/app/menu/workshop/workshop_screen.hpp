#pragma once

#include <SDL3/SDL_keycode.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <vector>

#include "app/menu/builder_screen.hpp"
#include "app/menu/piece_models.hpp"
#include "app/menu/workshop/bench_drag.hpp"
#include "app/menu/workshop/bench_pick.hpp"
#include "app/menu/workshop/orbit_camera.hpp"
#include "app/menu/workshop/workshop_panels.hpp"
#include "assets/car_data.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/car_build/bench.hpp"
#include "sim/car_build/built_car.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

/// `from` is where it was lifted from, or nothing for a new piece.
struct HeldPiece {
    const assets::PieceRecord* piece = nullptr;
    std::uint8_t colour = 0;
    std::uint16_t partType = 0;
    unsigned rotation = 0;
    std::optional<BenchPiece> from;
};

struct BenchAim {
    BenchCell cell;
    sim::PiecePlacement placement;
    std::expected<int, sim::PlacementFailure> rest;
};

/// The full-window builder, with a free camera over a plain background. What `services` points
/// to must outlive it.
class WorkshopScreen final : public BuilderScreen {
public:
    [[nodiscard]] static Result<std::unique_ptr<WorkshopScreen>> Load(
        render::Renderer& renderer, const BuildServices& services, sim::BuiltCar car,
        std::vector<std::size_t> sets);

    void HandleKey(SDL_Keycode key, SDL_Keymod mod) override;
    [[nodiscard]] bool UseControls(BuildMode mode) override;
    [[nodiscard]] BuildOutcome Tick(const sim::InputFrame& input) override;
    [[nodiscard]] bool HoldsPointer() const override { return isDragging_; }
    [[nodiscard]] const sim::BuiltCar& Car() const override { return bench_.car; }

    void Render(std::vector<render::RenderView>& views,
                std::vector<render::SpriteDraw>& sprites) override;

private:
    WorkshopScreen(render::Renderer& renderer, const BuildServices& services, PieceModels models,
                   render::TextureId groundCell, sim::BuiltCar car, WorkshopPanels panels);

    void UseMode(BuildMode mode);
    void Steer(const sim::MenuPointer& pointer);
    void FrameCar();
    [[nodiscard]] Vector3 CarCorner() const;
    [[nodiscard]] Vector3 GroundCorner() const;
    [[nodiscard]] render::Camera ViewCamera() const;
    void AddLooseBricks();

    [[nodiscard]] BenchView ViewNow() const;
    void TickEdits(const sim::MenuPointer& pointer, bool isOverPanel);
    void Act(const PanelTick& panel);
    void Grab(const PaletteEntry& entry);
    void ChooseSwatch(std::uint8_t colour);
    [[nodiscard]] BenchPiece Newest(sim::BenchArea area) const;
    void Lift(const BenchPiece& piece);
    void Aim(const sim::MenuPointer& pointer, bool isOverPanel);
    void Drop();
    void PutBack();
    void Discard();
    void Turn(int quarters);
    void FlashBlockers(const BenchPiece& piece);
    bool Apply(const sim::BenchEdit& edit);
    void Paint(std::uint8_t colour);
    void DeleteSelected();
    void StepHistory(bool undoes);
    [[nodiscard]] bool Allows(const assets::PieceRecord& piece, sim::BenchArea area) const;
    [[nodiscard]] BuildOutcome Outcome();
    [[nodiscard]] Vector3 CornerOf(sim::BenchArea area, sim::PiecePlacement placement,
                                   int height) const;
    void AddShell(const sim::CarPiece& piece, const Vector3& corner, const render::Color& tint,
                  render::BlendMode blend);
    void AddHeld();
    void AddSelection();
    void AddBlockers();

    render::Renderer* renderer_;
    BuildServices services_;
    PieceModels models_;
    render::TextureId groundCell_;
    sim::Bench bench_;
    Vector3 carOffset_;
    OrbitCamera camera_;
    WorkshopPanels panels_;
    bool showsHint_ = true;
    bool isDragging_ = false;
    bool isDone_ = false;
    sim::BenchHistory history_;
    BenchDrag drag_;
    std::optional<HeldPiece> held_;
    std::optional<sim::Bench> beforeLift_;
    std::optional<BenchAim> aim_;
    std::optional<BenchPiece> selected_;
    bool isOverfull_ = false;
    assets::CarData shownCar_{};
    Milliseconds pulse_{0};
    std::vector<std::size_t> blockers_;
    Milliseconds flash_{0};
    std::vector<render::MeshDraw> draws_;
    std::array<render::QuadDraw, 2> quads_{};
    std::array<render::DirectionalLight, 2> lights_{};
};

}
