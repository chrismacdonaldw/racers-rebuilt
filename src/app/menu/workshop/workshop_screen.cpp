#include "app/menu/workshop/workshop_screen.hpp"

#include <algorithm>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "app/frame_loop.hpp"
#include "app/menu/builder_settings.hpp"
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
constexpr std::string_view kPanelFont = "fontmenu";
constexpr render::DirectionalLight kKeyLight{.direction = {.x = -0.36F, .y = -0.86F, .z = -0.36F}};
constexpr render::DirectionalLight kFillLight{.direction = {.x = 0.6F, .y = -0.3F, .z = 0.7F},
                                              .color = {.r = 0.35F, .g = 0.35F, .b = 0.4F}};

sim::MenuPointer Stilled(sim::MenuPointer pointer) {
    if (pointer.LeftPressed()) {
        pointer.pressed = false;
    }
    pointer.wheel = 0.0F;
    return pointer;
}

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
                                                             sim::BuiltCar car,
                                                             std::vector<std::size_t> sets) {
    Result<PieceModels> models =
        PieceModels::Load(renderer, services.stand->Loader(), *services.catalog);
    Result<render::TextureId> groundCell = models.and_then(
        [&](const PieceModels&) { return renderer.UploadTexture(GroundCellImage()); });
    Result<std::vector<UiImage>> icons = groundCell.and_then(
        [&](render::TextureId) { return LoadSetIcons(renderer, *services.vfs); });
    const std::string_view language =
        services.text != nullptr ? services.text->Language() : std::string_view{};
    if (!icons) {
        return Fail(icons.error().code, std::format("workshop: {}", icons.error().message));
    }
    if (sets.empty()) {
        return Fail(ErrorCode::InvalidData, "workshop: there are no part sets to build from");
    }
    WorkshopPanels panels(*services.catalog, std::move(sets), std::move(*icons),
                          ListBackdropTracks(*services.vfs, language));
    return std::unique_ptr<WorkshopScreen>(new WorkshopScreen(
        renderer, services, std::move(*models), *groundCell, std::move(car), std::move(panels)));
}

WorkshopScreen::WorkshopScreen(render::Renderer& renderer, BuildServices services,
                               PieceModels models, render::TextureId groundCell, sim::BuiltCar car,
                               WorkshopPanels panels)
    : renderer_(&renderer),
      services_(std::move(services)),
      models_(std::move(models)),
      groundCell_(groundCell),
      bench_{.car = std::move(car)},
      carOffset_(CarOffset(bench_.car, *services_.catalog)),
      camera_({.focus = kCarAt + carOffset_ + kGridMiddle,
               .yaw = kHomeYaw,
               .pitch = kHomePitch,
               .distance = kHomeDistance}),
      panels_(std::move(panels)),
      shownCar_(bench_.car.Save()),
      lights_{kKeyLight, kFillLight} {
    panels_.Fit(renderer_->UiWindow());
    UseMode(services_.mode);
}

void WorkshopScreen::HandleKey(SDL_Keycode key, SDL_Keymod mod) {
    const bool isCtrl = (mod & SDL_KMOD_CTRL) != 0;
    if (key >= SDLK_0 && key <= SDLK_9) {
        ChooseSwatch(static_cast<std::uint8_t>((key - SDLK_0 + kColourKeys - 1) % kColourKeys));
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
            } else if (selected_) {
                selected_.reset();
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
    UseMode(mode);
    return true;
}

void WorkshopScreen::UseMode(const BuildMode& mode) {
    services_.mode = mode;
    if (mode.offersExtraPieces) {
        services_.catalog->ReadExtraModels(*services_.vfs);
    }
    panels_.OfferExtras(mode.offersExtraPieces);
    ShowBackground(services_.mode.background);
}

std::vector<SettingChange> WorkshopScreen::TakeSettingChanges() {
    return std::exchange(changes_, {});
}

void WorkshopScreen::ChooseBackground(const BackgroundChoice& choice) {
    BuildMode mode = services_.mode;
    if (choice.colour) {
        mode.backdrop = *choice.colour;
        changes_.push_back(
            {.key = std::string(kBuilderColour.key), .value = FormatHexColour(*choice.colour)});
    }
    mode.background = choice.background;
    changes_.push_back({.key = std::string(kBuilderBackground.key), .value = choice.background});
    UseMode(mode);
}

void WorkshopScreen::ShowBackground(std::string_view background) {
    backdrop_ = nullptr;
    if (background == kColourBackground) {
        return;
    }
    if (!panels_.Backgrounds().Offers(background)) {
        log::Warn("the workshop shows its colour, as {} isn't a race track", background);
        return;
    }
    auto found = backdrops_.find(background);
    if (found == backdrops_.end()) {
        const Vector3 spot = CarCorner() + Vector3{.x = kGridMiddle.x, .y = kGridMiddle.y};
        Result<std::unique_ptr<TrackBackdrop>> loaded =
            TrackBackdrop::Load(*renderer_, *services_.vfs, services_.stand->Loader(), background,
                                {.x = spot.x, .y = spot.y});
        if (!loaded) {
            log::Warn("the workshop shows its colour, not {}: {}", background,
                      loaded.error().message);
            return;
        }
        found = backdrops_.emplace(std::string(background), std::move(*loaded)).first;
    }
    backdrop_ = found->second.get();
}

BuildOutcome WorkshopScreen::Tick(const sim::InputFrame& input) {
    panels_.Fit(renderer_->UiWindow());
    camera_.Tick(kSimTick);
    const bool isFree = !drag_.IsBusy() && !isDragging_;
    const PanelTick panel = panels_.Tick(isFree ? input.pointer : Stilled(input.pointer));
    sim::MenuPointer pointer = input.pointer;
    if (panel.isOver && isFree) {
        Act(panel);
        pointer = Stilled(pointer);
    }
    Steer(pointer);
    TickEdits(pointer, panel.isOver);
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
    showsHint_ = showsHint_ && !isDragging_ && pointer.wheel == 0.0F;
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
    camera.farClip = backdrop_ != nullptr ? std::max(kFarPlane, backdrop_->FarPlane()) : kFarPlane;
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
                            std::vector<render::SpriteDraw>& sprites) {
    const render::Camera camera = ViewCamera();
    const render::Viewport viewport = render::UiViewport(panels_.Layout().scene);
    if (backdrop_ != nullptr) {
        views.push_back(backdrop_->View(camera, viewport));
    }
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
                     .quads = backdrop_ != nullptr ? std::span(quads_).subspan(1) : quads_,
                     .viewport = viewport});
    panels_.Render(models_,
                   {.font = services_.text != nullptr ? services_.text->Font(kPanelFont) : nullptr,
                    .pieces = bench_.car.Pieces().size(),
                    .plates = bench_.car.Grid().Tallest(),
                    .showsHint = showsHint_,
                    .background = services_.mode.background,
                    .backdrop = services_.mode.backdrop},
                   views, sprites);
}

}
