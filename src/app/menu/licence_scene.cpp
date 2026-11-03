#include "app/menu/licence_scene.hpp"

#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <utility>

#include "app/frame_loop.hpp"
#include "app/menu/garage_racers.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/ui_text/text_layout.hpp"
#include "assets/loader_pool.hpp"
#include "core/log.hpp"
#include "core/utf16_text.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kCharsetLine = 1;
constexpr std::size_t kTitleLine = 10;
constexpr std::size_t kDefaultNameLine = 0x3A;
constexpr std::size_t kExpressionLine = 0x3B;
constexpr std::size_t kBackLine = 0x1F;
constexpr std::size_t kDiscardLine = 0x7B;
constexpr std::size_t kContinueLine = 0x20;
constexpr std::size_t kLoaderWorkers = 2;
constexpr std::uint8_t kExpressions = 6;
constexpr std::array<std::size_t, 3> kEditSounds{5, 7, 8};
constexpr Milliseconds kBlinkHalf{1000};
constexpr float kCursorHeight = 4.0F;
constexpr float kCursorRise = 6.0F;
constexpr char16_t kLastLatin1 = 0xFF;

Result<std::vector<std::string>> CircuitNames(const assets::Vfs& vfs) {
    return assets::RaceCatalog::Load(vfs).transform([](const assets::RaceCatalog& catalog) {
        std::vector<std::string> names;
        for (const assets::CircuitEntry& circuit : catalog.Circuits()) {
            names.push_back(circuit.name);
        }
        return names;
    });
}

std::unique_ptr<DriverStand> LoadStand(render::Renderer& renderer, const assets::Vfs& vfs,
                                       assets::TrackLoader& loader, const MenuFiles& files) {
    Result<std::unique_ptr<DriverStand>> stand =
        DriverStand::Load(renderer, vfs, loader, files, kLicenceScreen, kLicenceStandSet);
    if (!stand) {
        log::Warn("{}, the licence shows no photo", stand.error().message);
        return nullptr;
    }
    return std::move(*stand);
}

std::optional<DialogLook> LoadDialog(render::Renderer& renderer, const assets::Vfs& vfs) {
    Result<DialogLook> look = LoadDialogLook(renderer, vfs);
    if (!look) {
        log::Warn("{}, the licence discards changes without asking", look.error().message);
        return std::nullopt;
    }
    return std::move(*look);
}

std::optional<char16_t> TypedOf(const SDL_KeyboardEvent& key) {
    if (key.key == SDLK_SPACE ||
        (key.key > SDLK_SPACE && key.key <= kLastLatin1 && key.key != SDLK_DELETE)) {
        return LicenceUpper(static_cast<char16_t>(key.key));
    }
    return std::nullopt;
}

float WidestGlyph(const UiFont& font, std::u16string_view allowed) {
    float widest = 0.0F;
    for (std::size_t at = 0; at < allowed.size(); ++at) {
        widest = std::max(widest, TextWidth(font, allowed.substr(at, 1)));
    }
    return widest;
}

}

Result<std::unique_ptr<LicenceScene>> LicenceScene::Create(render::Renderer& renderer,
                                                           const assets::Vfs& vfs,
                                                           MenuSounds sounds,
                                                           const std::filesystem::path& racersFile,
                                                           EditedRacer racer, sim::Cheats& cheats) {
    Result<settings::RacerStore> store = LoadRacerStore(racersFile, racer.index);
    Result<std::vector<std::string>> circuits =
        store.and_then([&](const settings::RacerStore&) { return CircuitNames(vfs); });
    Result<MenuFiles> menu = circuits.and_then(
        [&](const std::vector<std::string>&) { return ReadMenuFiles(vfs, kLicenceScreen); });
    Result<LicenceLook> look = menu.and_then([&](const MenuFiles& loaded) {
        return LoadLicenceLook(renderer, vfs, loaded, ButtonsOf(RacerEditor::Licence, racer.isNew));
    });
    if (!look) {
        return Fail(look.error().code, std::format("licence: {}", look.error().message));
    }
    Views views{.pool = std::make_unique<assets::LoaderPool>(kLoaderWorkers)};
    views.loader = std::make_unique<assets::TrackLoader>(vfs, *views.pool);
    views.stand = LoadStand(renderer, vfs, *views.loader, *menu);
    views.dialog = LoadDialog(renderer, vfs);
    Racers racers{.file = racersFile,
                  .store = std::move(*store),
                  .racer = racer,
                  .circuits = std::move(*circuits)};
    auto scene = std::unique_ptr<LicenceScene>(new LicenceScene(
        renderer, vfs, sounds, std::move(*look), std::move(views), std::move(racers)));
    scene->cheats_ = &cheats;
    return scene;
}

LicenceScene::LicenceScene(render::Renderer& renderer, const assets::Vfs& vfs, MenuSounds sounds,
                           LicenceLook look, Views views, Racers racers)
    : renderer_(&renderer),
      vfs_(&vfs),
      sounds_(sounds),
      look_(std::move(look)),
      views_(std::move(views)),
      racers_(std::move(racers)),
      buttons_(ButtonsOf(RacerEditor::Licence, racers_.racer.isNew)),
      name_(look_.name.maxLength),
      expression_(racers_.store.racers.at(racers_.racer.index).look.expression) {
    LayOut();
    ShowPhoto();
}

void LicenceScene::ApplySettings(const settings::Registry& settings) {
    static_cast<void>(text_.Apply(*renderer_, *vfs_, settings));
    name_.Allow(std::u16string(text_.Line(kCharsetLine)));
    if (!isNamed_) {
        isNamed_ = true;
        const std::u16string saved = Utf16FromUtf8(Record().name);
        name_.SetText(saved.empty() ? text_.Line(kDefaultNameLine) : saved);
    }
    const UiFont* font = text_.Font(look_.name.font);
    cursorWidth_ = font != nullptr ? WidestGlyph(*font, name_.Allowed()) : 0.0F;
    if (dialog_) {
        dialog_->Measure(text_);
    }
}

std::optional<std::string> LicenceScene::TakeNextScene() {
    return std::exchange(opened_, std::nullopt);
}

void LicenceScene::HandleEvent(const SDL_Event& event) {
    if (event.type != SDL_EVENT_KEY_DOWN || menu_.Focus() != LicenceRow::Name || dialog_) {
        return;
    }
    if (event.key.key == SDLK_BACKSPACE) {
        keys_.emplace_back(std::nullopt);
        isBackTyped_ = true;
    } else if (const std::optional<char16_t> typed = TypedOf(event.key)) {
        keys_.emplace_back(*typed);
        isConfirmTyped_ = isConfirmTyped_ || event.key.key == SDLK_SPACE;
    }
}

void LicenceScene::Tick(const sim::InputFrame& input) {
    if (views_.stand) {
        views_.stand->Tick(kSimTick);
    }
    blink_ = (blink_ + kSimTick) % (kBlinkHalf * 2);
    if (dialog_) {
        TickDialog(*dialog_, input);
        return;
    }
    sim::InputFrame menuInput = input;
    if (std::exchange(isConfirmTyped_, false)) {
        menuInput.menuPressed.reset(sim::InputFrame::Index(sim::MenuAction::Confirm));
    }
    if (std::exchange(isBackTyped_, false)) {
        menuInput.menuPressed.reset(sim::InputFrame::Index(sim::MenuAction::Back));
    }
    TypeKeys();
    const LicenceRow focusBefore = menu_.Focus();
    AimPointer();
    const PointerChoice pointed = pointer_.Tick(input.pointer, targets_);
    if (const std::optional<std::size_t> focus = pointed.Focus()) {
        menu_.FocusOn(static_cast<LicenceRow>(*focus));
    }
    menuInput = WithClick(menuInput, pointed);
    const LicenceRequest request = menu_.Tick(menuInput);
    Handle(request);
    PlaySounds(menuInput, request, focusBefore);
}

void LicenceScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = (static_cast<std::uint64_t>(menu_.Focus()) << 8U) | expression_;
    out.entityClocks.clear();
    if (views_.stand) {
        out.entityClocks.assign(views_.stand->Clocks().begin(), views_.stand->Clocks().end());
    }
}

void LicenceScene::Render(const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    render::RenderView view;
    if (views_.stand) {
        DrawPicturesAround(under_, views_.stand->Framing().interior, sprites_);
        view = views_.stand->Render(interpolated.entityClocks);
        DrawMenuFrameBorder(views_.stand->Framing().look, views_.stand->Framing().frame, sprites_);
    } else {
        DrawPictures(under_, sprites_);
    }
    DrawBanner(text_, look_.banner, text_.Line(kTitleLine), look_.backdrop, look_.bannerTop,
               sprites_);
    DrawName();
    DrawLabel(text_, look_.expressionButton.at(StateOf(LicenceRow::Expression)),
              text_.Line(kExpressionLine), look_.expression.x, look_.expression.y, sprites_);
    DrawImageButton(text_, look_.done, StateOf(LicenceRow::Done), text_.Line(buttons_.done.line),
                    look_.donePlace, sprites_);
    DrawImageButton(text_, look_.back, StateOf(LicenceRow::Back), text_.Line(buttons_.back.line),
                    look_.backPlace, sprites_);
    if (dialog_) {
        dialog_->Draw(text_, sprites_);
    }
    view.sprites = sprites_;
    Result<void> drawn = renderer_->Draw(view);
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

void LicenceScene::Handle(LicenceRequest request) {
    switch (request) {
        case LicenceRequest::Expression:
            expression_ = static_cast<std::uint8_t>((expression_ + 1) % kExpressions);
            ShowPhoto();
            break;
        case LicenceRequest::Done:
            SaveThenOpen(NextScene(RacerEditor::Licence, racers_.racer));
            break;
        case LicenceRequest::Leave:
            AskLeave();
            break;
        case LicenceRequest::None:
            break;
    }
}

void LicenceScene::TickDialog(MessageDialog& dialog, const sim::InputFrame& input) {
    if (dialog.Tick(input, sounds_) == 0) {
        opened_ = PreviousScene(RacerEditor::Licence, racers_.racer);
    }
    if (dialog.IsClosed()) {
        dialog_.reset();
    }
}

void LicenceScene::TypeKeys() {
    for (const std::optional<char16_t> key : std::exchange(keys_, {})) {
        const NameEdit edit = key ? name_.Type(*key) : name_.Erase();
        sounds_.Play(look_.name.sounds.at(kEditSounds.at(static_cast<std::size_t>(edit))));
    }
}

void LicenceScene::SaveThenOpen(std::string next) {
    if (Save()) {
        opened_ = std::move(next);
    }
}

bool LicenceScene::Save() {
    if (cheats_ != nullptr) {
        cheats_->EnterPlate(PlateText(name_.Text()));
    }
    if (name_.Text().empty()) {
        return false;
    }
    settings::Racer& racer = Record();
    racer.name = Utf8FromUtf16(name_.Text());
    racer.look.expression = expression_;
    if (Result<void> saved = settings::SaveRacers(racers_.store, racers_.file); !saved) {
        log::Warn("{}, the licence isn't saved", saved.error().message);
    }
    return true;
}

void LicenceScene::AskLeave() {
    if (racers_.racer.isNew) {
        SaveThenOpen(PreviousScene(RacerEditor::Licence, racers_.racer));
        return;
    }
    if (!HasChanges() || !views_.dialog) {
        opened_ = PreviousScene(RacerEditor::Licence, racers_.racer);
        return;
    }
    dialog_.emplace(
        *views_.dialog, kDiscardLine,
        std::vector<DialogButton>{{.line = kContinueLine},
                                  {.line = kBackLine, .style = DialogButtonStyle::Cross}});
    dialog_->Measure(text_);
}

void LicenceScene::ShowPhoto() {
    if (!views_.stand) {
        return;
    }
    auto photo = photos_.find(expression_);
    if (photo == photos_.end()) {
        assets::DriverLook look = Record().look;
        look.expression = expression_;
        Result<StandRacer> built = BuildStandDriver(*renderer_, *vfs_, *views_.loader, look);
        if (!built) {
            log::Warn("{}, the licence photo isn't updated", built.error().message);
            return;
        }
        photo = photos_.emplace(expression_, std::move(*built)).first;
    }
    const StandRacer& shown = photo->second;
    Result<void> placed = views_.stand->Show(
        {.model = shown.driver, .skeleton = &shown.skeleton, .scale = shown.scale});
    if (!placed) {
        log::Warn("{}, the licence photo isn't updated", placed.error().message);
    }
}

bool LicenceScene::HasChanges() const {
    const settings::Racer& racer = racers_.store.racers.at(racers_.racer.index);
    return name_.Text() != Utf16FromUtf8(racer.name) || expression_ != racer.look.expression;
}

settings::Racer& LicenceScene::Record() {
    return racers_.store.racers.at(racers_.racer.index);
}

void LicenceScene::PlaySounds(const sim::InputFrame& input, LicenceRequest request,
                              LicenceRow focusBefore) const {
    if (menu_.Focus() != focusBefore) {
        sounds_.MoveHighlight(SoundsOf(focusBefore), SoundsOf(menu_.Focus()));
    } else if (input.WasPressed(sim::MenuAction::Back)) {
        sounds_.Confirm(look_.back.sounds);
    } else if (request != LicenceRequest::None) {
        sounds_.Confirm(SoundsOf(focusBefore));
    }
}

const assets::SoundIds& LicenceScene::SoundsOf(LicenceRow row) const {
    switch (row) {
        case LicenceRow::Expression:
            return look_.expressionSounds;
        case LicenceRow::Done:
            return look_.done.sounds;
        case LicenceRow::Back:
            return look_.back.sounds;
        case LicenceRow::Name:
            return look_.name.focusSounds;
    }
    return look_.name.focusSounds;
}

std::size_t LicenceScene::StateOf(LicenceRow row) const {
    return menu_.Focus() == row ? kFocusedState : kNormalState;
}

void LicenceScene::LayOut() {
    under_.push_back(look_.backdrop);
    under_.push_back(look_.licence);
    const settings::Racer& racer = racers_.store.racers.at(racers_.racer.index);
    const std::size_t shown = std::min(racers_.circuits.size(), kLicenceTrophies);
    for (std::size_t circuit = 0; circuit < shown; ++circuit) {
        const auto trophy = racer.trophies.find(racers_.circuits.at(circuit));
        if (trophy != racer.trophies.end()) {
            under_.push_back(TrophyPicture(look_, circuit, trophy->second));
        }
    }
    DrawMenuFrame(look_.name.frame, look_.name.frameBounds, sprites_);
    for (const render::SpriteDraw& piece : sprites_) {
        under_.push_back({.texture = piece.texture, .bounds = piece.bounds, .tint = piece.tint});
    }
    sprites_.clear();
}

void LicenceScene::AimPointer() {
    targets_.clear();
    targets_.push_back({.area = look_.name.frameBounds,
                        .control = static_cast<std::size_t>(LicenceRow::Name),
                        .click = std::nullopt});
    targets_.push_back(
        {.area = LabelArea(text_, look_.expressionButton.at(StateOf(LicenceRow::Expression)),
                           text_.Line(kExpressionLine), look_.expression),
         .control = static_cast<std::size_t>(LicenceRow::Expression)});
    targets_.push_back({.area = ImageButtonArea(text_, look_.done, StateOf(LicenceRow::Done),
                                                text_.Line(buttons_.done.line), look_.donePlace),
                        .control = static_cast<std::size_t>(LicenceRow::Done)});
    targets_.push_back({.area = ImageButtonArea(text_, look_.back, StateOf(LicenceRow::Back),
                                                text_.Line(buttons_.back.line), look_.backPlace),
                        .control = static_cast<std::size_t>(LicenceRow::Back)});
}

void LicenceScene::DrawName() {
    const UiFont* font = text_.Font(look_.name.font);
    if (font == nullptr) {
        return;
    }
    const render::Rect& box = look_.name.bounds;
    LayoutText(*font, name_.Text(), {.x = box.x, .y = box.y}, sprites_);
    const bool isBlinkOn = blink_ < kBlinkHalf;
    if (menu_.Focus() == LicenceRow::Name && !name_.IsFull() && isBlinkOn && !dialog_) {
        sprites_.push_back({.bounds = {.x = box.x + TextWidth(*font, name_.Text()),
                                       .y = box.y + box.height - kCursorRise,
                                       .width = cursorWidth_,
                                       .height = kCursorHeight}});
    }
}

}
