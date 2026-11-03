#include "app/menu/car_build_screen.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/frame_loop.hpp"
#include "app/menu/edit_car_look.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/racer/racer_bounds.hpp"
#include "assets/world_database.hpp"
#include "core/log.hpp"
#include "core/quaternion.hpp"
#include "render/ui_layout.hpp"
#include "sim/car_build/car_mesh.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kHelpFile = "CARBUILD.SRF";
constexpr Vector3 kSpot{.z = 1.0F};
constexpr Vector3 kUp{.z = 1.0F};
constexpr Vector3 kLookAbove{.z = 4.0F};
constexpr float kPlateHeight = 0.4F;
constexpr render::Color kWhite{};
constexpr std::array<std::optional<std::size_t>, kBuildRows> kRowHelp{0, 1, 2, 6, 3, 4, 5, {}};
constexpr auto kFirstButtonRow = static_cast<std::size_t>(BuildRow::Move);
constexpr std::int32_t kMoveSound = 16;
constexpr std::int32_t kBlockedSound = 8;
constexpr std::int32_t kRotateSound = 22;
constexpr std::int32_t kRefusedSound = 18;
constexpr std::int32_t kPlacedSound = 10;
constexpr std::int32_t kUndoSound = 13;
constexpr std::int32_t kViewSound = 6;
constexpr std::int32_t kCarViewSound = 5;
constexpr std::int32_t kPieceViewSound = 17;
constexpr float kViewVolume = 0.6F;
constexpr float kBlockedViewVolume = 0.7F;

std::vector<std::uint8_t> ChoiceIndices(const assets::PieceCatalog& catalog, std::size_t set) {
    std::vector<std::uint8_t> choices(catalog.partSets.at(set).choices.size());
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

std::size_t ButtonState(const CarBuildMenu& menu, std::size_t button) {
    const auto row = static_cast<BuildRow>(button + kFirstButtonRow);
    return menu.Focus() == row ? kFocusedState : kNormalState;
}

std::vector<std::u16string> ReadHelp(const assets::Vfs& vfs, std::string_view language) {
    Result<std::vector<std::u16string>> lines = ReadMenuText(vfs, language, kHelpFile);
    if (!lines) {
        log::Warn("the car builder has no help text: {}", lines.error().message);
    }
    return std::move(lines).value_or(std::vector<std::u16string>{});
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
      carousel_(MakeCarousel(*services_.catalog, builder_.Set(), look_, 0)),
      carouselView_(look_.carousel, look_.itemColors) {
    under_.push_back(look_.backdrop);
    holes_ = {look_.carousel.bounds, look_.garage.interior};
}

void CarBuildScreen::HandleKey(SDL_Keycode key) {
    if (const std::optional<BuildRequest> request = BuildKeyRequest(key)) {
        keys_.push_back(*request);
    }
}

BuildOutcome CarBuildScreen::Tick(const sim::InputFrame& input) {
    carousel_.Tick(kSimTick);
    camera_.Tick(kSimTick);
    const bool pressed = !keys_.empty() || input.menuPressed.any();
    BuildOutcome outcome = motion_.IsBusy() ? BuildOutcome::None : ApplyInput(input);
    keys_.clear();
    outcome = std::max(outcome, Animate());
    TickHelp(pressed);
    return outcome;
}

BuildOutcome CarBuildScreen::ApplyInput(const sim::InputFrame& input) {
    BuildOutcome outcome = BuildOutcome::None;
    sim::InputFrame menuInput = input;
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
    PadSounds(held);
    return outcome;
}

BuildOutcome CarBuildScreen::Apply(const BuildRequest& request) {
    switch (request.command) {
        case BuildCommand::PreviousSet:
        case BuildCommand::NextSet:
            StepSet(request.command == BuildCommand::NextSet ? 1 : -1);
            break;
        case BuildCommand::PreviousPiece:
        case BuildCommand::NextPiece:
            if (carousel_.Step(request.command == BuildCommand::NextPiece ? 1 : -1) ==
                CarouselStep::Moved) {
                builder_.Choose(carousel_.Part());
            }
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
        case BuildCommand::Rotate:
            builder_.Rotate();
            services_.sounds.Play(kRotateSound);
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
    set_ = (set_ + (direction > 0 ? 1 : count - 1)) % count;
    builder_.SelectSet(sets_.at(set_));
    ShowSet();
}

void CarBuildScreen::ShowSet() {
    carousel_ = MakeCarousel(*services_.catalog, builder_.Set(), look_, builder_.Choice());
}

void CarBuildScreen::Move(std::size_t direction) {
    services_.sounds.Play(builder_.Move(direction, camera_.Slot()) ? kMoveSound : kBlockedSound);
}

void CarBuildScreen::View(bool moved) const {
    if (moved) {
        services_.sounds.Play(kViewSound, kViewVolume);
    } else {
        services_.sounds.Play(kBlockedSound, kBlockedViewVolume);
    }
}

BuildOutcome CarBuildScreen::Place() {
    switch (builder_.Check()) {
        case PlaceResult::Placed:
            motion_.Drop();
            break;
        case PlaceResult::TooManyPieces:
            return BuildOutcome::TooManyPieces;
        case PlaceResult::Refused:
            services_.sounds.Play(kRefusedSound);
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
        static_cast<void>(builder_.Undo());
        static_cast<void>(Refit());
        return BuildOutcome::TooManyPieces;
    }
    services_.sounds.Play(kPlacedSound);
    return BuildOutcome::CarChanged;
}

bool CarBuildScreen::Undo() {
    const std::size_t set = builder_.Set();
    const std::size_t choice = builder_.Choice();
    if (!builder_.Undo()) {
        services_.sounds.Play(kRefusedSound);
        return false;
    }
    services_.sounds.Play(kUndoSound);
    static_cast<void>(Refit());
    motion_.Lift(RestHeight());
    if (builder_.Set() == set && builder_.Choice() == choice) {
        return true;
    }
    const auto found = std::ranges::find(sets_, builder_.Set());
    if (found != sets_.end()) {
        set_ = static_cast<std::size_t>(found - sets_.begin());
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
    fit_ = {.offset = *offset, .radius = ModelBounds(mesh->geometry.vertices).radius};
    return mesh->drawnPieces >= builder_.Car().Pieces().size();
}

float CarBuildScreen::RestHeight() const {
    const assets::PieceRecord* piece = builder_.Cursor().Piece();
    if (piece == nullptr) {
        return 0.0F;
    }
    const sim::PiecePlacement at = builder_.Cursor().Placement();
    const assets::CellSize size = piece->shape.Footprint(at.rotation);
    int height = 0;
    for (int y = 0; y < size.height; ++y) {
        for (int x = 0; x < size.width; ++x) {
            if (piece->shape.GetCell(x, y, at.rotation).Occupied()) {
                height = std::max(height, builder_.Car().Grid().At(at.x + x, at.y + y).height);
            }
        }
    }
    return static_cast<float>(height) * kPlateHeight;
}

BuildOutcome CarBuildScreen::Animate() {
    const bool fits = builder_.Check() == PlaceResult::Placed;
    if (motion_.Tick(kSimTick, fits, RestHeight()) == PieceMotionEnd::Dropped) {
        return Land();
    }
    return BuildOutcome::None;
}

void CarBuildScreen::PadSounds(std::optional<BuildPad> before) const {
    const std::optional<BuildPad> after = menu_.Held();
    if (before == after) {
        return;
    }
    const BuildPad pad = after.value_or(before.value_or(BuildPad::Move));
    services_.sounds.Play(pad == BuildPad::Move ? kPieceViewSound : kCarViewSound);
}

void CarBuildScreen::TickHelp(bool pressed) {
    if (const std::string_view language = services_.text->Language();
        !language.empty() && language != helpLanguage_) {
        helpLanguage_ = language;
        helpLines_ = ReadHelp(*services_.vfs, language);
        helpBox_.reset();
    }
    const auto row = static_cast<std::size_t>(menu_.Focus());
    const std::optional<std::size_t> line = kRowHelp.at(row);
    const std::u16string_view text = line && *line < helpLines_.size()
                                         ? std::u16string_view(helpLines_[*line])
                                         : std::u16string_view{};
    help_.Tick(kSimTick, row, pressed, !text.empty());
    if (!help_.IsShown()) {
        helpBox_.reset();
        return;
    }
    if (helpBox_ && helpRow_ == row) {
        return;
    }
    const UiFont* font = services_.text->Font(kBuildHelpFont);
    if (font == nullptr || text.empty()) {
        helpBox_.reset();
        return;
    }
    helpBox_ = LayoutHelp(*font, text, RowBounds(row),
                          {.width = render::kUiWidth, .height = render::kUiHeight});
    helpRow_ = row;
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

void CarBuildScreen::AddPiece(float height, std::optional<float> alpha) {
    const sim::PiecePlacement at = builder_.Cursor().Placement();
    const PartPreview& model =
        models_.Model(*builder_.Cursor().Piece(), builder_.Colour(), at.rotation);
    const Vector3 corner{.x = fit_.offset.x + static_cast<float>(at.x),
                         .y = fit_.offset.y + static_cast<float>(at.y),
                         .z = fit_.offset.z + height};
    for (render::MeshDraw draw : model.draws) {
        draw.transform = {.position = corner};
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
    camera.position = camera_.Eye(fit_.radius);
    camera.target = kSpot + kLookAbove;
    camera.fovDegrees = scene.verticalFovDegrees;
    camera.nearClip = scene.nearPlane;
    camera.farClip = scene.farPlane;
    return MenuSceneCamera(camera);
}

void CarBuildScreen::Render(std::vector<render::RenderView>& views,
                            std::vector<render::SpriteDraw>& sprites) {
    DrawPicturesAround(under_, holes_, sprites);
    extras_.clear();
    if (builder_.Cursor().Piece() != nullptr) {
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
    const Transform car{.position = kSpot, .rotation = FromAxisAngle(kUp, camera_.Angle())};
    views.push_back(services_.stand->Render(car, {.camera = ViewCamera(),
                                                  .interior = look_.garage.interior,
                                                  .light = kWhite,
                                                  .extras = extras_}));
    DrawMenuFrameBorder(look_.garage.look, look_.garage.frame, sprites);
    carouselView_.Render(carousel_, models_.SetPreviews(builder_.Set()),
                         menu_.Focus() == BuildRow::Pieces, under_, views);
    DrawControls(sprites);
}

void CarBuildScreen::DrawControls(std::vector<render::SpriteDraw>& sprites) const {
    DrawSelectorArrows(look_.arrows, look_.sets, menu_.Focus() == BuildRow::Sets, sprites);
    DrawSelectorArrows(look_.arrows, look_.pieces, menu_.Focus() == BuildRow::Pieces, sprites);
    const UiImage& icon = look_.setImages.at(builder_.Set());
    const render::Rect sized = SizedAt({}, icon);
    sprites.push_back({.texture = icon.texture,
                       .bounds = {.x = look_.setBox.x + ((look_.setBox.width - sized.width) / 2),
                                  .y = look_.setBox.y + ((look_.setBox.height - sized.height) / 2),
                                  .width = sized.width,
                                  .height = sized.height}});
    for (std::size_t index = 0; index < kBuildButtons; ++index) {
        const StateButton& shown = look_.buttons.at(index);
        sprites.push_back(
            {.texture = shown.images.at(ButtonState(menu_, index)), .bounds = shown.bounds});
    }
    const UiFont* font = services_.text->Font(kBuildHelpFont);
    if (helpBox_ && font != nullptr) {
        DrawHelp(*font, *helpBox_, sprites);
    }
}

}
