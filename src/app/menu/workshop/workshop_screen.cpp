#include "app/menu/workshop/workshop_screen.hpp"

#include <format>
#include <utility>

#include "app/frame_loop.hpp"
#include "app/menu/grid_pick.hpp"
#include "app/menu/stand_scene.hpp"
#include "app/menu/workshop/workshop_scenery.hpp"
#include "app/track_space.hpp"
#include "assets/world_database.hpp"
#include "core/log.hpp"
#include "render/ui_layout.hpp"
#include "sim/car_build/car_mesh.hpp"
#include "sim/car_build/loose_bricks.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kCarAt{.z = 1.0F};
constexpr Vector3 kGridMiddle{.x = 5.0F, .y = 3.0F, .z = 1.5F};
constexpr float kCarRadius = 6.0F;
constexpr float kFovDegrees = 50.0F;
constexpr float kNearPlane = 0.1F;
constexpr float kFarPlane = 400.0F;
constexpr float kHomeYaw = -0.9F;
constexpr float kHomePitch = 0.45F;
constexpr float kHomeDistance = 18.0F;
constexpr float kTurnPerPx = 0.006F;
constexpr float kPanPerPx = 0.0015F;
constexpr int kColourKeys = 10;
constexpr render::DirectionalLight kKeyLight{.direction = {.x = -0.36F, .y = -0.86F, .z = -0.36F}};
constexpr render::DirectionalLight kFillLight{.direction = {.x = 0.6F, .y = -0.3F, .z = 0.7F},
                                              .color = {.r = 0.35F, .g = 0.35F, .b = 0.4F}};

Vector3 CarOffset(const sim::BuiltCar& car, const assets::PieceCatalog& catalog) {
    Result<Vector3> offset = sim::CarMeshOffset(car, catalog);
    if (!offset) {
        log::Warn("{}, the workshop puts the grid at the car's origin", offset.error().message);
        return {};
    }
    return *offset;
}

}

Result<std::unique_ptr<WorkshopScreen>> WorkshopScreen::Load(render::Renderer& renderer,
                                                             const BuildServices& services,
                                                             sim::BuiltCar car) {
    Result<PieceModels> models =
        PieceModels::Load(renderer, services.stand->Loader(), *services.catalog);
    Result<render::TextureId> groundCell = models.and_then(
        [&](const PieceModels&) { return renderer.UploadTexture(GroundCellImage()); });
    if (!groundCell) {
        return Fail(groundCell.error().code,
                    std::format("workshop: {}", groundCell.error().message));
    }
    return std::unique_ptr<WorkshopScreen>(
        new WorkshopScreen(renderer, services, std::move(*models), *groundCell, std::move(car)));
}

WorkshopScreen::WorkshopScreen(render::Renderer& renderer, const BuildServices& services,
                               PieceModels models, render::TextureId groundCell, sim::BuiltCar car)
    : renderer_(&renderer),
      services_(services),
      models_(std::move(models)),
      groundCell_(groundCell),
      bench_{.car = std::move(car)},
      carOffset_(CarOffset(bench_.car, *services_.catalog)),
      camera_({.focus = kCarAt + carOffset_ + kGridMiddle,
               .yaw = kHomeYaw,
               .pitch = kHomePitch,
               .distance = kHomeDistance}),
      shownCar_(bench_.car.Save()),
      lights_{kKeyLight, kFillLight} {}

void WorkshopScreen::HandleKey(SDL_Keycode key, SDL_Keymod mod) {
    const bool isCtrl = (mod & SDL_KMOD_CTRL) != 0;
    if (key >= SDLK_0 && key <= SDLK_9) {
        Paint(static_cast<std::uint8_t>((key - SDLK_0 + kColourKeys - 1) % kColourKeys));
        return;
    }
    switch (key) {
        case SDLK_F:
            FrameCar();
            break;
        case SDLK_HOME:
            camera_.Home();
            break;
        case SDLK_R:
            Turn(1);
            break;
        case SDLK_DELETE:
        case SDLK_BACKSPACE:
            DeleteSelected();
            break;
        case SDLK_Z:
            if (isCtrl) {
                StepHistory((mod & SDL_KMOD_SHIFT) == 0);
            }
            break;
        case SDLK_Y:
            if (isCtrl) {
                StepHistory(false);
            }
            break;
        case SDLK_ESCAPE:
            if (held_) {
                PutBack();
            } else {
                isDone_ = true;
            }
            break;
        case SDLK_RETURN:
            isDone_ = true;
            break;
        default:
            break;
    }
}

bool WorkshopScreen::UseControls(BuildMode mode) {
    if (mode.controls != BuildControls::Workshop) {
        return false;
    }
    services_.mode = mode;
    return true;
}

BuildOutcome WorkshopScreen::Tick(const sim::InputFrame& input) {
    camera_.Tick(kSimTick);
    Steer(input.pointer);
    TickEdits(input.pointer);
    return Outcome();
}

void WorkshopScreen::Steer(const sim::MenuPointer& pointer) {
    const bool pans = pointer.IsHeld(sim::PointerButton::Middle) ||
                      (pointer.IsHeld(sim::PointerButton::Right) && pointer.isShifted);
    if (pans) {
        camera_.Pan(pointer.dx * kPanPerPx, -pointer.dy * kPanPerPx);
    } else if (pointer.IsHeld(sim::PointerButton::Right)) {
        camera_.Turn(-pointer.dx * kTurnPerPx, pointer.dy * kTurnPerPx);
    }
    if (held_ && pointer.wheel != 0.0F) {
        Turn(pointer.wheel > 0.0F ? 1 : -1);
    } else {
        camera_.Zoom(pointer.wheel);
    }
    isDragging_ =
        pointer.IsHeld(sim::PointerButton::Right) || pointer.IsHeld(sim::PointerButton::Middle);
}

void WorkshopScreen::FrameCar() {
    camera_.Frame({.centre = kCarAt + carOffset_ + kGridMiddle, .radius = kCarRadius}, kFovDegrees);
}

Vector3 WorkshopScreen::CarCorner() const {
    return kCarAt + carOffset_;
}

Vector3 WorkshopScreen::GroundCorner() const {
    const Vector3 grid = CarCorner();
    return {.x = grid.x - static_cast<float>(sim::kCarOnGround.x),
            .y = grid.y - static_cast<float>(sim::kCarOnGround.y)};
}

render::Camera WorkshopScreen::ViewCamera() const {
    assets::WorldCamera camera;
    camera.position = camera_.Eye();
    camera.target = camera_.Focus();
    camera.fovDegrees = kFovDegrees;
    camera.nearClip = kNearPlane;
    camera.farClip = kFarPlane;
    return MenuSceneCamera(camera);
}

void WorkshopScreen::AddLooseBricks() {
    const Vector3 corner = GroundCorner();
    for (const sim::CarPiece& piece : bench_.ground.Pieces()) {
        const Vector3 at = corner + Vector3{.x = static_cast<float>(piece.placement.x),
                                            .y = static_cast<float>(piece.placement.y),
                                            .z = static_cast<float>(piece.height) * kPlateHeight};
        for (render::MeshDraw draw :
             models_.Model(*piece.piece, piece.colour, piece.placement.rotation).draws) {
            draw.transform = TransformToEngine({.position = at});
            draws_.push_back(draw);
        }
    }
}

void WorkshopScreen::Render(std::vector<render::RenderView>& views,
                            [[maybe_unused]] std::vector<render::SpriteDraw>& sprites) {
    const render::Camera camera = ViewCamera();
    draws_.clear();
    services_.stand->AddCarDraws({.position = kCarAt}, {}, draws_);
    AddLooseBricks();
    AddSelection();
    AddBlockers();
    AddHeld();
    quads_ = {BackdropQuad(camera, services_.mode.backdrop),
              GroundQuad(groundCell_, GroundCorner(), static_cast<float>(sim::kGroundSize.width),
                         static_cast<float>(sim::kGroundSize.height))};
    views.push_back({.camera = camera,
                     .lighting = {.lights = lights_},
                     .meshes = draws_,
                     .quads = quads_,
                     .viewport = render::UiViewport(renderer_->UiWindow())});
}

}
