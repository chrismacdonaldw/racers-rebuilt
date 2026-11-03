#include "app/menu/car_build_screen.hpp"

#include <algorithm>
#include <format>
#include <numbers>
#include <utility>

#include "app/frame_loop.hpp"
#include "app/menu/edit_car_look.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/racer/racer_bounds.hpp"
#include "assets/world_database.hpp"
#include "core/log.hpp"
#include "core/quaternion.hpp"
#include "sim/car_build/car_mesh.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kView = 1;
constexpr float kViewAngle = 1.57F + (static_cast<float>(kView) * std::numbers::pi_v<float> / 4.0F);
constexpr Vector3 kSpot{.z = 1.0F};
constexpr Vector3 kUp{.z = 1.0F};
constexpr Vector3 kLookAbove{.z = 4.0F};
constexpr Vector3 kNearEye{.y = -10.0F, .z = 14.0F};
constexpr Vector3 kFarEye{.y = -12.0F, .z = 17.0F};
constexpr float kNearRadius = 5.9F;
constexpr float kFarRadius = 8.5F;
constexpr float kPlateHeight = 0.4F;
constexpr int kHeldPlates = 21;
constexpr float kGhostAlpha = 100.0F / 255.0F;
constexpr render::Color kWhite{};

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
    const auto row = static_cast<BuildRow>(button + static_cast<std::size_t>(BuildRow::Move));
    return menu.Focus() == row ? kFocusedState : kNormalState;
}

}

Result<std::unique_ptr<CarBuildScreen>> CarBuildScreen::Load(
    render::Renderer& renderer, const assets::Vfs& vfs, CarStand& stand,
    const assets::PieceCatalog& catalog, sim::BuiltCar car, std::vector<std::size_t> sets) {
    Result<MenuFiles> files = ReadMenuFiles(vfs, kCarBuildScreen);
    Result<CarBuildLook> look = files.and_then([&](MenuFiles& loaded) {
        return AddBuilderImages(vfs, loaded).and_then([&] {
            return LoadCarBuildLook(renderer, vfs, loaded);
        });
    });
    Result<PieceModels> models = look.and_then(
        [&](const CarBuildLook&) { return PieceModels::Load(renderer, stand.Loader(), catalog); });
    if (!models) {
        return Fail(models.error().code, std::format("car builder: {}", models.error().message));
    }
    if (sets.empty()) {
        return Fail(ErrorCode::InvalidData, "car builder: there are no part sets to build from");
    }
    CarBuilder builder(catalog, std::move(car), sets.front());
    auto screen = std::unique_ptr<CarBuildScreen>(new CarBuildScreen(
        stand, catalog, std::move(*look), std::move(*models), std::move(builder), std::move(sets)));
    if (!screen->Refit()) {
        log::Warn("the car builder can't measure the car, so it frames it as small");
    }
    return screen;
}

CarBuildScreen::CarBuildScreen(CarStand& stand, const assets::PieceCatalog& catalog,
                               CarBuildLook look, PieceModels models, CarBuilder builder,
                               std::vector<std::size_t> sets)
    : stand_(&stand),
      catalog_(&catalog),
      look_(std::move(look)),
      models_(std::move(models)),
      builder_(std::move(builder)),
      sets_(std::move(sets)),
      carousel_(MakeCarousel(catalog, builder_.Set(), look_, 0)),
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
    keys_.clear();
    return std::max(outcome, Apply(menu_.Tick(menuInput)));
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
            builder_.Move(request.direction, kView);
            break;
        case BuildCommand::Rotate:
            builder_.Rotate();
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
    carousel_ = MakeCarousel(*catalog_, builder_.Set(), look_, builder_.Choice());
}

BuildOutcome CarBuildScreen::Place() {
    const PlaceResult placed = builder_.Place();
    if (placed == PlaceResult::TooManyPieces) {
        return BuildOutcome::TooManyPieces;
    }
    if (placed == PlaceResult::Refused) {
        return BuildOutcome::None;
    }
    if (!Refit()) {
        static_cast<void>(builder_.Undo());
        static_cast<void>(Refit());
        return BuildOutcome::TooManyPieces;
    }
    return BuildOutcome::CarChanged;
}

bool CarBuildScreen::Undo() {
    const std::size_t set = builder_.Set();
    const std::size_t choice = builder_.Choice();
    if (!builder_.Undo()) {
        return false;
    }
    static_cast<void>(Refit());
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
    Result<sim::CarMesh> mesh = sim::BuildCarMesh(builder_.Car(), *catalog_);
    Result<Vector3> offset = mesh.and_then(
        [&](const sim::CarMesh&) { return sim::CarMeshOffset(builder_.Car(), *catalog_); });
    if (!offset) {
        log::Warn("{}, the car builder keeps its last framing", offset.error().message);
        return true;
    }
    fit_ = {.offset = *offset, .radius = ModelBounds(mesh->geometry.vertices).radius};
    return mesh->drawnPieces >= builder_.Car().Pieces().size();
}

int CarBuildScreen::RestHeight() const {
    const assets::PieceRecord* piece = builder_.Cursor().Piece();
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
    return height;
}

void CarBuildScreen::AddPiece(float height, std::optional<float> alpha) {
    const sim::PiecePlacement at = builder_.Cursor().Placement();
    const PartPreview& model =
        models_.Model(*builder_.Cursor().Piece(), builder_.Colour(), at.rotation);
    const Vector3 corner{.x = fit_.offset.x + static_cast<float>(at.x),
                         .y = fit_.offset.y + static_cast<float>(at.y),
                         .z = fit_.offset.z + (height * kPlateHeight)};
    for (render::MeshDraw draw : model.draws) {
        draw.transform = {.position = corner};
        if (alpha) {
            draw.material.blend = render::BlendMode::Alpha;
            draw.material.tint.a = *alpha;
        }
        extras_.push_back(draw);
    }
}

render::Camera CarBuildScreen::BuildCamera() const {
    const float blend =
        std::clamp((fit_.radius - kNearRadius) / (kFarRadius - kNearRadius), 0.0F, 1.0F);
    const render::Camera& scene = stand_->SceneCamera();
    assets::WorldCamera camera;
    camera.position = kNearEye + ((kFarEye - kNearEye) * blend);
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
        AddPiece(static_cast<float>(kHeldPlates), std::nullopt);
        AddPiece(static_cast<float>(RestHeight()), kGhostAlpha);
    }
    const Transform car{.position = kSpot, .rotation = FromAxisAngle(kUp, kViewAngle)};
    views.push_back(stand_->Render(car, {.camera = BuildCamera(),
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
}

}
