#include "app/menu/race_setup_scene.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <span>
#include <string_view>
#include <utility>

#include "app/frame_loop.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/race_settings.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "MAINMENU";
constexpr std::string_view kTitleFont = "fontmenu";
constexpr std::string_view kCourseSelector = "imagecon";
constexpr std::string_view kRaceButton = "buttonch";
constexpr std::string_view kBackButton = "buttonla";
constexpr std::string_view kPicker = "picker";
constexpr std::size_t kCircuitTitleLine = 13;
constexpr std::size_t kSingleRaceTitleLine = 14;
constexpr std::size_t kVersusTitleLine = 0x19;
constexpr std::size_t kTimeTrialTitleLine = 0x0F;
constexpr std::size_t kVersusPlayers = 2;
constexpr std::size_t kBackLine = 26;
constexpr std::size_t kRaceLine = 27;
constexpr std::size_t kHostingLine = 0x4A;
constexpr std::size_t kCircuitNumberLine = 0x4B;
constexpr std::size_t kFirstChampionLine = 0x4C;
constexpr std::size_t kFirstNumberLine = 0x61;
constexpr std::size_t kDisabledState = 0;
constexpr std::size_t kDisabledFocusedState = 1;
constexpr float kLeft = 3.0F;
constexpr float kValueLeft = 230.0F;
constexpr float kTitleTop = 118.0F;
constexpr float kChampionTop = 278.0F;
constexpr std::array kRowTops{178.0F, 358.0F, 418.0F};
constexpr float kBestTypeTop = 268.0F;
constexpr float kBestTimeTop = 308.0F;
constexpr float kTrialStatusLeft = 251.0F;
constexpr float kTrialStatusTop = 414.0F;
constexpr float kStackedValueDrop = 24.0F;
constexpr std::size_t kFinalCircuit = 6;
constexpr std::array kScrollSteps{std::pair{sim::MenuAction::Left, -1},
                                  std::pair{sim::MenuAction::Right, 1}};

std::u16string Widen(std::string_view text) {
    std::u16string wide;
    for (const char letter : text) {
        wide.push_back(static_cast<char16_t>(static_cast<unsigned char>(letter)));
    }
    return wide;
}

std::u16string OneLine(std::u16string_view text) {
    std::u16string line(text);
    std::ranges::replace(line, u'\n', u' ');
    return line;
}

std::u16string Lookup(const std::vector<std::u16string>& names, std::int32_t index,
                      std::string_view fallback) {
    const auto at = static_cast<std::size_t>(index);
    return index >= 0 && at < names.size() ? names[at] : Widen(fallback);
}

std::size_t TitleLine(RaceSetupMode mode) {
    switch (mode) {
        case RaceSetupMode::Circuit:
            return kCircuitTitleLine;
        case RaceSetupMode::Versus:
            return kVersusTitleLine;
        case RaceSetupMode::TimeTrial:
            return kTimeTrialTitleLine;
        case RaceSetupMode::SingleRace:
            return kSingleRaceTitleLine;
    }
    return kSingleRaceTitleLine;
}

RaceSetupLook LookOf(const MenuFiles& files, std::vector<MenuPicture> pictures) {
    return {
        .pictures = std::move(pictures),
        .states = TextButtonStates(files),
        .rowSounds = {SelectorSounds(files, kCourseSelector), GameButtonSounds(files, kRaceButton),
                      GameButtonSounds(files, kBackButton)},
        .scrollSounds = ScrollSounds(files, kPicker)};
}

Result<std::vector<CourseChoice>> LoadCourses(const assets::Vfs& vfs, RaceSetupMode mode,
                                              const std::set<std::string>& unlockedCircuits) {
    switch (mode) {
        case RaceSetupMode::Circuit:
            return LoadCircuitChoices(vfs, unlockedCircuits);
        case RaceSetupMode::TimeTrial:
            return LoadTrialChoices(vfs, unlockedCircuits);
        default:
            return LoadTrackChoices(vfs);
    }
}

std::string_view PreviewLayout(RaceSetupMode mode) {
    return mode == RaceSetupMode::Circuit ? "CIRCRACE" : "SINGRACE";
}

std::unique_ptr<TrackPreview> LoadPreview(const RaceOutputs& outputs, const assets::Vfs& vfs,
                                          std::string_view language, RaceSetupMode mode) {
    if (mode == RaceSetupMode::Versus) {
        return nullptr;
    }
    Result<std::unique_ptr<TrackPreview>> preview =
        TrackPreview::Load(outputs, vfs, language, PreviewLayout(mode));
    if (!preview) {
        log::Warn("race setup shows no preview: {}", preview.error().message);
        return nullptr;
    }
    return std::move(*preview);
}

}

std::optional<RaceSetupMode> RaceSetupModeOf(std::string_view scene) {
    if (scene == kSingleRaceScene) {
        return RaceSetupMode::SingleRace;
    }
    if (scene == kCircuitRaceScene) {
        return RaceSetupMode::Circuit;
    }
    if (scene == kVersusRaceScene) {
        return RaceSetupMode::Versus;
    }
    if (scene == kTimeTrialScene) {
        return RaceSetupMode::TimeTrial;
    }
    return std::nullopt;
}

Result<std::unique_ptr<RaceSetupScene>> RaceSetupScene::Create(
    const RaceOutputs& outputs, const assets::Vfs& vfs, MenuSounds sounds, RaceSetupMode mode,
    const settings::Progress& progress, std::string_view language) {
    render::Renderer& renderer = *outputs.renderer;
    Result<MenuFiles> files = ReadMenuFiles(vfs, kScreen);
    Result<MenuPage> page = files.and_then([&](const MenuFiles& read) {
        return BuildMenuPage(renderer, vfs, read, std::span<const ButtonLine>{});
    });
    Result<std::vector<CourseChoice>> courses = page.and_then(
        [&](const MenuPage&) { return LoadCourses(vfs, mode, progress.unlockedCircuits); });
    if (!courses) {
        return std::unexpected(std::move(courses.error()));
    }
    auto scene = std::unique_ptr<RaceSetupScene>(new RaceSetupScene(
        renderer, vfs, sounds, mode, LookOf(*files, std::move(page->pictures)),
        RaceSetupMenu(std::move(*courses), mode == RaceSetupMode::Versus ? kVersusPlayers : 1)));
    scene->progress_ = progress;
    scene->ShowBestTimes();
    scene->preview_ = LoadPreview(outputs, vfs, language, mode);
    scene->ShowPreview();
    return scene;
}

void RaceSetupScene::ApplySettings(const settings::Registry& settings) {
    rules_ = {.respawn = RespawnRuleFrom(settings), .aiDriving = AiDrivingFrom(settings)};
    if (mode_ == RaceSetupMode::SingleRace) {
        rules_.opponents = static_cast<std::size_t>(settings.Get(kOpponents));
    }
    if (mode_ == RaceSetupMode::Versus) {
        rules_.lapCount = settings.Get(kVersusLaps);
    }
    rules_.isTimeTrial = mode_ == RaceSetupMode::TimeTrial;
    if (!text_.Apply(*renderer_, *vfs_, settings)) {
        return;
    }
    if (mode_ == RaceSetupMode::Circuit) {
        courseLabel_ = OneLine(text_.Line(kCircuitNumberLine));
    }

    if (mode_ != RaceSetupMode::Circuit) {
        const Result<std::vector<std::u16string>> races = ReadRaceNames(*vfs_, text_.Language());
        if (!races) {
            log::Warn("races show their folders: {}", races.error().message);
        }
        raceNames_ = races.value_or(std::vector<std::u16string>{});
    }
    courseNames_ = CourseNames();
}

std::optional<std::string> RaceSetupScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

void RaceSetupScene::Tick(const sim::InputFrame& input) {
    const RaceSetupRow focusBefore = menu_.Focus();
    const std::size_t courseBefore = menu_.CourseIndex();
    AimPointer();
    const PointerChoice pointed = pointer_.Tick(input.pointer, targets_);
    if (const std::optional<std::size_t> focus = pointed.Focus()) {
        menu_.FocusOn(static_cast<RaceSetupRow>(*focus));
    }
    const sim::InputFrame clicked = WithClick(input, pointed);
    std::optional<std::string> next = menu_.Tick(clicked, rules_);
    PlaySounds(clicked, focusBefore);
    if (menu_.CourseIndex() != courseBefore) {
        ShowBestTimes();
        ShowPreview();
    }
    bestTimes_.Tick(kSimTick);
    if (preview_) {
        preview_->Tick(kSimTick);
    }
    if (next) {
        next_ = std::move(next);
    }
}

void RaceSetupScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = menu_.StateHash();
    out.entityClocks.clear();
    if (preview_) {
        out.entityClocks.assign(preview_->Clocks().begin(), preview_->Clocks().end());
    }
}

void RaceSetupScene::Render(const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    render::RenderView view;
    if (preview_) {
        DrawPicturesAround(look_.pictures, preview_->Bounds(), sprites_);
        view = preview_->Render(interpolated.entityClocks, sprites_);
    } else {
        DrawPictures(look_.pictures, sprites_);
    }
    const bool isCircuit = mode_ == RaceSetupMode::Circuit;

    DrawLabel(text_, {.font = std::string(kTitleFont)}, text_.Line(TitleLine(mode_)), kLeft,
              kTitleTop, sprites_);
    DrawRow(RaceSetupRow::Course);
    if (isCircuit) {
        DrawChampion();
    }
    if (mode_ == RaceSetupMode::TimeTrial) {
        DrawBestTimes();
    }
    DrawRow(RaceSetupRow::Race);
    DrawRow(RaceSetupRow::Back);
    view.sprites = sprites_;
    Result<void> drawn = renderer_->Draw(view);
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

RaceSetupScene::RaceSetupScene(render::Renderer& renderer, const assets::Vfs& vfs,
                               MenuSounds sounds, RaceSetupMode mode, RaceSetupLook look,
                               RaceSetupMenu menu)
    : renderer_(&renderer),
      vfs_(&vfs),
      sounds_(sounds),
      mode_(mode),
      look_(std::move(look)),
      menu_(std::move(menu)),
      courseNames_(CourseNames()) {}

void RaceSetupScene::PlaySounds(const sim::InputFrame& input, RaceSetupRow focusBefore) const {
    const auto soundsOf = [&](RaceSetupRow row) {
        const bool isDisabled = row == RaceSetupRow::Race && menu_.Course().isLocked;
        return isDisabled ? assets::SoundIds{} : look_.rowSounds.at(static_cast<std::size_t>(row));
    };
    if (input.WasPressed(sim::MenuAction::Back)) {
        sounds_.Confirm(soundsOf(RaceSetupRow::Back));
        return;
    }
    const RaceSetupRow focus = menu_.Focus();
    if (focus != focusBefore) {
        sounds_.MoveHighlight(soundsOf(focusBefore), soundsOf(focus));
    }
    if (focus == RaceSetupRow::Course) {
        for (const auto& [action, step] : kScrollSteps) {
            if (input.WasPressed(action)) {
                sounds_.Scroll(look_.scrollSounds, step);
            }
        }
    }
    if (input.WasPressed(sim::MenuAction::Confirm)) {
        sounds_.Confirm(soundsOf(focus));
    }
}

std::vector<std::u16string> RaceSetupScene::CourseNames() const {
    std::vector<std::u16string> names;
    for (const CourseChoice& course : menu_.Courses()) {
        names.push_back(mode_ == RaceSetupMode::Circuit
                            ? std::u16string(text_.Line(kFirstNumberLine + course.circuitIndex))
                            : Lookup(raceNames_, course.nameLine, course.track));
    }
    return names;
}

void RaceSetupScene::ShowPreview() {
    if (!preview_) {
        return;
    }
    const CourseChoice& course = menu_.Course();
    const bool isCircuit = mode_ == RaceSetupMode::Circuit;
    preview_->Show(
        course.previews,
        {.fades = isCircuit, .cycles = isCircuit && course.circuitIndex != kFinalCircuit},
        course.isLocked);
}

RaceSetupScene::RowText RaceSetupScene::TextOf(RaceSetupRow row) const {
    switch (row) {
        case RaceSetupRow::Course:
            return {.label = courseLabel_,
                    .value = courseNames_.at(menu_.CourseIndex()),
                    .isGameText = mode_ == RaceSetupMode::Circuit};
        case RaceSetupRow::Race:
            return {.label = text_.Line(kRaceLine)};
        default:
            return {.label = text_.Line(kBackLine)};
    }
}

std::size_t RaceSetupScene::StateOf(RaceSetupRow row) const {
    const bool isFocused = menu_.Focus() == row;
    if (row == RaceSetupRow::Race && menu_.Course().isLocked) {
        return isFocused ? kDisabledFocusedState : kDisabledState;
    }
    return isFocused ? kFocusedState : kNormalState;
}

MenuPoint RaceSetupScene::ValueAt(float top) const {
    return preview_ ? MenuPoint{.x = kLeft, .y = top + kStackedValueDrop}
                    : MenuPoint{.x = kValueLeft, .y = top};
}

render::Rect RaceSetupScene::RowArea(RaceSetupRow row) const {
    const RowText text = TextOf(row);
    const MenuButtonState& look = look_.states.at(StateOf(row));
    const MenuPoint at{.x = kLeft, .y = kRowTops.at(static_cast<std::size_t>(row))};
    if (preview_ && !text.isGameText && !text.value.empty()) {
        return LabelArea(text_, look, text.value, at);
    }
    const render::Rect label = LabelArea(text_, look, text.label, at);
    if (text.value.empty()) {
        return label;
    }
    return Spanning(label, LabelArea(text_, look, text.value, ValueAt(at.y)));
}

void RaceSetupScene::AimPointer() {
    targets_.clear();
    for (const RaceSetupRow row : {RaceSetupRow::Course, RaceSetupRow::Race, RaceSetupRow::Back}) {
        if (row == RaceSetupRow::Race && menu_.Course().isLocked) {
            continue;
        }
        targets_.push_back({.area = RowArea(row),
                            .control = static_cast<std::size_t>(row),
                            .click = row == RaceSetupRow::Course ? sim::MenuAction::Right
                                                                 : sim::MenuAction::Confirm});
    }
}

void RaceSetupScene::DrawRow(RaceSetupRow row) {
    const RowText text = TextOf(row);
    const float top = kRowTops.at(static_cast<std::size_t>(row));
    const MenuButtonState& look = look_.states.at(StateOf(row));
    if (preview_ && !text.isGameText && !text.value.empty()) {
        DrawLabel(text_, look, text.value, kLeft, top, sprites_);
        return;
    }
    DrawLabel(text_, look, text.label, kLeft, top, sprites_);
    DrawValue(look, text.value, top);
}

void RaceSetupScene::DrawValue(const MenuButtonState& state, std::u16string_view value, float top) {
    const MenuPoint at = ValueAt(top);
    DrawLabel(text_, state, value, at.x, at.y, sprites_);
}

void RaceSetupScene::DrawChampion() {
    const MenuButtonState& state = look_.states.at(kNormalState);
    DrawLabel(text_, state, OneLine(text_.Line(kHostingLine)), kLeft, kChampionTop, sprites_);
    DrawValue(state, OneLine(text_.Line(kFirstChampionLine + menu_.Course().circuitIndex)),
              kChampionTop);
}

void RaceSetupScene::ShowBestTimes() {
    const CourseChoice& course = menu_.Course();
    const auto records = progress_.bestTimes.find(course.track);
    bestTimes_.Show(
        records == progress_.bestTimes.end() ? settings::TrackRecords{} : records->second,
        progress_.beatenRecords.contains(static_cast<std::int32_t>(course.race)));
}

void RaceSetupScene::DrawBestTimes() {
    const MenuButtonState& state = look_.states.at(kNormalState);
    const BestTimeRows rows = bestTimes_.Rows(text_);
    DrawLabel(text_, state, rows.type, kLeft, kBestTypeTop, sprites_);
    DrawValue(state, rows.name, kBestTypeTop);
    DrawValue(state, rows.time, preview_ ? kBestTypeTop + kStackedValueDrop : kBestTimeTop);
    DrawLabel(text_, state, rows.status, kTrialStatusLeft, kTrialStatusTop, sprites_);
}
}
