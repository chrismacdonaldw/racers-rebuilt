#include "app/menu/car_build_screen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/frame_loop.hpp"
#include "app/menu/build_sounds.hpp"
#include "app/menu/edit_car_look.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/racer/racer_bounds.hpp"
#include "app/track_space.hpp"
#include "assets/world_database.hpp"
#include "core/log.hpp"
#include "core/quaternion.hpp"
#include "render/ui_layout.hpp"
#include "sim/car_build/car_mesh.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kSpot{.z = 1.0F};
constexpr Vector3 kUp{.z = 1.0F};
constexpr Vector3 kLookAbove{.z = 4.0F};
constexpr render::Color kWhite{};
constexpr auto kFirstButtonRow = static_cast<std::size_t>(BuildRow::Move);
constexpr float kViewVolume = 0.6F;
constexpr float kBlockedViewVolume = 0.7F;
constexpr float kFullTurn = 2.0F * std::numbers::pi_v<float>;
constexpr float kItemTilt = kFullTurn * 114.0F / 1024.0F;
constexpr float kLitLevel = 180.0F / 120.0F;
constexpr render::Color kLitAmbient{.r = kLitLevel, .g = kLitLevel, .b = kLitLevel};

std::vector<std::uint8_t> ChoiceIndices(const assets::PieceCatalog& catalog, std::size_t set) {
    std::vector<std::uint8_t> choices(catalog.Choices(set).size());
    for (std::size_t choice = 0; choice < choices.size(); ++choice) {
        choices[choice] = static_cast<std::uint8_t>(choice);
    }
    return choices;
}

PartCarousel MakeCarousel(const assets::PieceCatalog& catalog, std::size_t set,
                          const CarBuildLook& look, std::size_t choice) {
    return {ChoiceIndices(catalog, set),
            static_cast<std::uint8_t>(choice),
            {.slots = look.carousel.slots.size(), .focused = look.carousel.focused},
            look.scroll};
}

Quaternion ItemTurn(float spin) {
    const Vector3 axis{.x = std::sin(kItemTilt), .z = std::cos(kItemTilt)};
    return FromAxisAngle(axis, spin) * FromAxisAngle(Vector3{.y = 1.0F}, kItemTilt);
}

std::size_t ButtonState(const CarBuildMenu& menu, std::size_t button) {
    const auto row = static_cast<BuildRow>(button + kFirstButtonRow);
    return menu.Focus() == row ? kFocusedState : kNormalState;
}

}

Result<std::unique_ptr<CarBuildScreen>> CarBuildScreen::Load(render::Renderer& renderer,
                                                             const assets::Vfs& vfs,
                                                             BuildServices services,
                                                             sim::BuiltCar car,
                                                             std::vector<std::size_t> sets) {
    Result<MenuFiles> files = ReadMenuFiles(vfs, kCarBuildScreen);
    Result<CarBuildLook> look = files.and_then([&](MenuFiles& loaded) {
        return AddBuilderImages(vfs, loaded).and_then([&] {
            return LoadCarBuildLook(renderer, vfs, loaded);
        });
    });
    Result<PieceModels> models = look.and_then([&](const CarBuildLook&) {
        return PieceModels::Load(renderer, services.stand->Loader(), *services.catalog);
    });
    if (!models) {
        return Fail(models.error().code, std::format("car builder: {}", models.error().message));
    }
    if (sets.empty()) {
        return Fail(ErrorCode::InvalidData, "car builder: there are no part sets to build from");
    }
    CarBuilder builder(*services.catalog, std::move(car), sets.front());
    auto screen = std::unique_ptr<CarBuildScreen>(new CarBuildScreen(
        services, std::move(*look), std::move(*models), std::move(builder), std::move(sets)));
    if (!screen->Refit()) {
        log::Warn("the car builder can't measure the car, so it frames it as small");
    }
    return screen;
}

CarBuildScreen::CarBuildScreen(BuildServices services, CarBuildLook look, PieceModels models,
                               CarBuilder builder, std::vector<std::size_t> sets)
    : services_(services),
      look_(std::move(look)),
      models_(std::move(models)),
      builder_(std::move(builder)),
      sets_(std::move(sets)),
      setIcons_(SetIcons(*services_.catalog, sets_)),
      carousel_(MakeCarousel(*services_.catalog, builder_.Set(), look_, 0)),
      carouselView_(look_.carousel, look_.itemColors),
      setStrip_(look_.setSlide),
      help_(*services_.vfs, *services_.text) {
    setStrip_.Snap(0, sets_.size());
    under_.push_back(look_.backdrop);
    holes_ = {look_.carousel.bounds, look_.garage.interior};
    static_cast<void>(UseControls(services_.mode));
}

void CarBuildScreen::HandleKey(SDL_Keycode key, SDL_Keymod mod) {
    if (const std::optional<PieceEditKey> edit =
            palette_ ? PieceEditKeyOf(key, mod) : std::nullopt) {
        edits_.push_back(*edit);
    } else if (const std::optional<BuildRequest> request = BuildKeyRequest(key)) {
        keys_.push_back(*request);
    } else if (palette_ && key == SDLK_R) {
        keys_.push_back({.command = BuildCommand::Rotate});
    }
}

BuildOutcome CarBuildScreen::Tick(const sim::InputFrame& input) {
    carousel_.Tick(kSimTick);
    camera_.Tick(kSimTick);
    setStrip_.Tick(kSimTick);
    spin_ = std::fmod(spin_ + (look_.spinPerMs * static_cast<float>(kSimTick.count())), kFullTurn);
    const bool pressed = !keys_.empty() || input.menuPressed.any();
    BuildOutcome outcome = BuildOutcome::None;
    if (motion_.IsBusy()) {
        pointer_.TickIdle(input.pointer);
        if (const std::optional<BuildPad> held = menu_.Held(); held && input.pointer.released) {
            menu_.LetGo();
            ChangePad(held);
        }
    } else {
        outcome = palette_ ? TickPalette(input) : ApplyInput(input);
    }
    keys_.clear();
    edits_.clear();
    outcome = std::max(outcome, Animate());
    if (!palette_) {
        const auto row = static_cast<std::size_t>(menu_.Focus());
        help_.Tick(row, RowBounds(row), pressed);
    }
    return outcome;
}

BuildOutcome CarBuildScreen::ApplyInput(const sim::InputFrame& input) {
    BuildOutcome outcome = BuildOutcome::None;
    const BuildPointing pointing = pointer_.Tick(input, look_, menu_);
    sim::InputFrame menuInput = pointing.input;
    if (!keys_.empty()) {
        for (const sim::MenuAction action : {sim::MenuAction::Up, sim::MenuAction::Down,
                                             sim::MenuAction::Left, sim::MenuAction::Right}) {
            menuInput.menuPressed.reset(sim::InputFrame::Index(action));
        }
    }
    for (const BuildRequest& key : keys_) {
        outcome = std::max(outcome, Apply(key));
    }
    const std::optional<BuildPad> held = menu_.Held();
    outcome = std::max(outcome, Apply(menu_.Tick(menuInput)));
    const SceneMouse mouse = drag_.Tick(input.pointer, Regions(), menu_);
    outcome = std::max(outcome, Apply(mouse.request));
    static_cast<void>(camera_.Swing(mouse.turn));
    static_cast<void>(camera_.Lift(mouse.tilt));
    ChangePad(held);
    ApplyPad(pointing.pad);
    return outcome;
}

void CarBuildScreen::ApplyPad(const BuildRequest& request) {
    bool moved = false;
    switch (request.command) {
        case BuildCommand::TurnLeft:
        case BuildCommand::TurnRight:
            moved = camera_.Turn(request.command == BuildCommand::TurnRight ? 1 : -1);
            break;
        case BuildCommand::TiltUp:
        case BuildCommand::TiltDown:
            moved = camera_.Tilt(request.command == BuildCommand::TiltUp ? 1 : -1);
            break;
        default:
            static_cast<void>(Apply(request));
            return;
    }
    if (moved) {
        services_.sounds.Play(kBuildViewSound, kViewVolume);
    }
}

BuildOutcome CarBuildScreen::Apply(const BuildRequest& request) {
    switch (request.command) {
        case BuildCommand::PreviousSet:
        case BuildCommand::NextSet:
            StepSet(request.command == BuildCommand::NextSet ? 1 : -1);
            break;
        case BuildCommand::PreviousPiece:
        case BuildCommand::NextPiece:
            StepPiece(request.command == BuildCommand::NextPiece ? 1 : -1);
            break;
        case BuildCommand::Move:
            Move(request.direction);
            break;
        case BuildCommand::TurnLeft:
        case BuildCommand::TurnRight:
            View(camera_.Turn(request.command == BuildCommand::TurnRight ? 1 : -1));
            break;
        case BuildCommand::TiltUp:
        case BuildCommand::TiltDown:
            View(camera_.Tilt(request.command == BuildCommand::TiltUp ? 1 : -1));
            break;
        case BuildCommand::ResetView:
            static_cast<void>(camera_.Reset());
            break;
        case BuildCommand::Rotate:
            builder_.Rotate();
            services_.sounds.Play(kBuildRotateSound);
            break;
        case BuildCommand::Place:
            return Place();
        case BuildCommand::Undo:
            return Undo() ? BuildOutcome::CarChanged : BuildOutcome::None;
        case BuildCommand::Done:
            return BuildOutcome::Done;
        case BuildCommand::None:
            break;
    }
    return BuildOutcome::None;
}

void CarBuildScreen::StepSet(int direction) {
    const std::size_t count = sets_.size();
    if (!setStrip_.Slide(direction, count)) {
        return;
    }
    services_.sounds.Scroll(look_.setSounds, direction);
    set_ = (set_ + (direction > 0 ? 1 : count - 1)) % count;
    builder_.SelectSet(sets_.at(set_));
    ShowSet();
    ShowPaletteSet();
}

void CarBuildScreen::StepPiece(int direction) {
    const CarouselStep step = carousel_.Step(direction);
    if (step == CarouselStep::Moved) {
        builder_.Choose(carousel_.Part());
        services_.sounds.Scroll(look_.pieceSounds, direction);
    } else if (step == CarouselStep::AtEnd) {
        services_.sounds.Play(look_.pieceSounds.back());
    }
}

void CarBuildScreen::ShowSet() {
    carousel_ = MakeCarousel(*services_.catalog, builder_.Set(), look_, builder_.Choice());
}

void CarBuildScreen::Move(std::size_t direction) {
    services_.sounds.Play(builder_.Move(direction, camera_.Slot()) ? kBuildMoveSound
                                                                   : kBuildBlockedSound);
}

void CarBuildScreen::View(bool moved) const {
    if (moved) {
        services_.sounds.Play(kBuildViewSound, kViewVolume);
    } else {
        services_.sounds.Play(kBuildBlockedSound, kBlockedViewVolume);
    }
}

BuildOutcome CarBuildScreen::Place() {
    switch (builder_.Check()) {
        case PlaceResult::Placed:
        case PlaceResult::TooManyPieces:
            motion_.Drop(RestHeight());
            break;
        case PlaceResult::Refused:
            services_.sounds.Play(kBuildRefusedSound);
            break;
    }
    return BuildOutcome::None;
}

BuildOutcome CarBuildScreen::Land() {
    const PlaceResult placed = builder_.Place();
    if (placed != PlaceResult::Placed) {
        return placed == PlaceResult::TooManyPieces ? BuildOutcome::TooManyPieces
                                                    : BuildOutcome::None;
    }
    if (!Refit()) {
        static_cast<void>(palette_ ? builder_.UndoEdit() : builder_.Undo());
        static_cast<void>(Refit());
        return BuildOutcome::TooManyPieces;
    }
    services_.sounds.Play(kBuildPlacedSound);
    if (palette_ && hold_ == PaletteHold::None) {
        builder_.Release();
    }
    return BuildOutcome::CarChanged;
}

bool CarBuildScreen::Undo() {
    const std::size_t set = builder_.Set();
    const std::size_t choice = builder_.Choice();
    if (!builder_.Undo()) {
        services_.sounds.Play(kBuildRefusedSound);
        return false;
    }
    services_.sounds.Play(kBuildUndoSound);
    static_cast<void>(Refit());
    motion_.Lift(RestHeight());
    if (builder_.Set() == set && builder_.Choice() == choice) {
        return true;
    }
    const auto found = std::ranges::find(sets_, builder_.Set());
    if (found != sets_.end()) {
        set_ = static_cast<std::size_t>(found - sets_.begin());
        setStrip_.Snap(set_, sets_.size());
    }
    ShowSet();
    return true;
}

bool CarBuildScreen::Refit() {
    Result<sim::CarMesh> mesh = sim::BuildCarMesh(builder_.Car(), *services_.catalog);
    Result<Vector3> offset = mesh.and_then([&](const sim::CarMesh&) {
        return sim::CarMeshOffset(builder_.Car(), *services_.catalog);
    });
    if (!offset) {
        log::Warn("{}, the car builder keeps its last framing", offset.error().message);
        return true;
    }
    fit_ = {.offset = *offset, .bounds = ModelBounds(mesh->geometry.vertices)};
    return mesh->drawnPieces >= builder_.Car().Pieces().size();
}

float CarBuildScreen::RestHeight() const {
    return static_cast<float>(builder_.RestPlates()) * kPlateHeight;
}

BuildOutcome CarBuildScreen::Animate() {
    const bool fits = builder_.Check() == PlaceResult::Placed;
    if (motion_.Tick(kSimTick, fits, RestHeight()) == PieceMotionEnd::Dropped) {
        return Land();
    }
    return BuildOutcome::None;
}

void CarBuildScreen::ChangePad(std::optional<BuildPad> before) {
    const std::optional<BuildPad> after = menu_.Held();
    if (before == after) {
        return;
    }
    if (before == BuildPad::View) {
        camera_.Settle(fit_.bounds.radius);
    }
    const BuildPad pad = after.value_or(before.value_or(BuildPad::Move));
    services_.sounds.Play(pad == BuildPad::Move ? kBuildPieceViewSound : kBuildCarViewSound);
}

BuildRegions CarBuildScreen::Regions() {
    BuildRegions regions{.scene = look_.garage.interior};
    if (menu_.Held()) {
        return regions;
    }
    const render::Camera camera = ViewCamera();
    const Transform car = CarTransform();
    const auto onScreen = [&](const BoundingSphere& inCar) {
        return SphereOnScreen(
            camera, regions.scene,
            {.centre = ToEngine(rr::Apply(car, inCar.centre)), .radius = inCar.radius});
    };
    regions.car = onScreen(fit_.bounds);
    if (const assets::PieceRecord* piece = builder_.Cursor().Piece()) {
        const sim::PiecePlacement at = builder_.Cursor().Placement();
        const BoundingSphere& bounds = models_.Model(*piece, builder_.Colour(), at.rotation).bounds;
        const float height = motion_.Layers(camera_.IsOverhead()).solid.value_or(RestHeight());
        regions.piece =
            onScreen({.centre = PieceCorner(height) + bounds.centre, .radius = bounds.radius});
    }
    return regions;
}

render::Rect CarBuildScreen::RowBounds(std::size_t row) const {
    switch (static_cast<BuildRow>(row)) {
        case BuildRow::Sets:
            return look_.sets.bounds;
        case BuildRow::Pieces:
            return look_.pieces.bounds;
        default:
            return look_.buttons.at(row - kFirstButtonRow).bounds;
    }
}

Vector3 CarBuildScreen::PieceCorner(float height) const {
    const sim::PiecePlacement at = builder_.Cursor().Placement();
    return {.x = fit_.offset.x + static_cast<float>(at.x),
            .y = fit_.offset.y + static_cast<float>(at.y),
            .z = fit_.offset.z + height};
}

Transform CarBuildScreen::CarTransform() const {
    return {.position = kSpot, .rotation = FromAxisAngle(kUp, camera_.Angle())};
}

void CarBuildScreen::AddPiece(float height, std::optional<float> alpha) {
    const PartPreview& model = models_.Model(*builder_.Cursor().Piece(), builder_.Colour(),
                                             builder_.Cursor().Placement().rotation);
    const bool isLit = drag_.Lit() == BuildRegion::Piece;
    for (render::MeshDraw draw : model.draws) {
        draw.transform = {.position = PieceCorner(height)};
        if (isLit) {
            draw.material.ambient = draw.material.ambient * kLitAmbient;
        }
        if (alpha) {
            draw.material.blend = render::BlendMode::Alpha;
            draw.material.tint.a *= *alpha;
            draw.material.alphaCutoff *= *alpha;
        }
        extras_.push_back(draw);
    }
}

render::Camera CarBuildScreen::ViewCamera() const {
    const render::Camera& scene = services_.stand->SceneCamera();
    assets::WorldCamera camera;
    camera.position = camera_.Eye(fit_.bounds.radius);
    camera.target = kSpot + kLookAbove;
    camera.fovDegrees = scene.verticalFovDegrees;
    camera.nearClip = scene.nearPlane;
    camera.farClip = scene.farPlane;
    return MenuSceneCamera(camera);
}

void CarBuildScreen::Render(std::vector<render::RenderView>& views,
                            std::vector<render::SpriteDraw>& sprites) {
    if (palette_) {
        RenderPalette(views, sprites);
        return;
    }
    DrawPicturesAround(under_, holes_, sprites);
    RenderScene(true, views);
    DrawMenuFrameBorder(look_.garage.look, look_.garage.frame, sprites);
    carouselView_.Render(carousel_, models_.SetPreviews(builder_.Set()),
                         menu_.Focus() == BuildRow::Pieces, under_, views, ItemTurn(spin_));
    DrawControls(sprites);
}

void CarBuildScreen::RenderScene(bool showsHeld, std::vector<render::RenderView>& views) {
    extras_.clear();
    if (palette_) {
        AddEditShells();
    }
    if (showsHeld && builder_.Cursor().Piece() != nullptr) {
        const HeldPieceLayers layers = motion_.Layers(camera_.IsOverhead());
        if (layers.trail) {
            AddPiece(layers.trail->height, layers.trail->alpha);
        }
        if (layers.ghost) {
            AddPiece(layers.ghost->height, layers.ghost->alpha);
        }
        if (layers.solid) {
            AddPiece(*layers.solid, std::nullopt);
        }
    }
    const bool isCarLit = drag_.Lit() == BuildRegion::Car;
    views.push_back(services_.stand->Render(
        CarTransform(), {.camera = ViewCamera(),
                         .interior = look_.garage.interior,
                         .light = kWhite,
                         .extras = extras_,
                         .carAmbient = isCarLit ? kLitAmbient : render::Color{}}));
}

void CarBuildScreen::DrawControls(std::vector<render::SpriteDraw>& sprites) const {
    DrawSelectorArrows(look_.arrows, look_.sets, menu_.Focus() == BuildRow::Sets, sprites);
    DrawSelectorArrows(look_.arrows, look_.pieces, menu_.Focus() == BuildRow::Pieces, sprites);
    DrawStrip(setStrip_, look_.setImages, setIcons_, look_.setBox, sprites);
    for (std::size_t index = 0; index < kBuildButtons; ++index) {
        const StateButton& shown = look_.buttons.at(index);
        sprites.push_back(
            {.texture = shown.images.at(ButtonState(menu_, index)), .bounds = shown.bounds});
    }
    pointer_.DrawPressed(look_, sprites);
    help_.Draw(sprites);
}

}
