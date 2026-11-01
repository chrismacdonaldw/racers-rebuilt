#include "app/menu/single_race_scene.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <utility>

#include "app/menu/menu_files.hpp"
#include "app/race_settings.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "MAINMENU";
constexpr std::string_view kTitleFont = "fontmenu";
constexpr std::size_t kTitleLine = 14;
constexpr std::size_t kRaceLine = 27;
constexpr std::size_t kBackLine = 26;
constexpr float kLeft = 3.0F;
constexpr float kValueLeft = 230.0F;
constexpr float kTitleTop = 118.0F;
constexpr std::array kRowTops{178.0F, 218.0F, 358.0F, 418.0F};

std::u16string Widen(std::string_view text) {
    std::u16string wide;
    for (const char letter : text) {
        wide.push_back(static_cast<char16_t>(static_cast<unsigned char>(letter)));
    }
    return wide;
}

std::vector<std::vector<std::u16string>> RacerNames(const std::vector<TrackChoice>& tracks,
                                                    const std::vector<std::u16string>& names) {
    std::vector<std::vector<std::u16string>> racerNames;
    for (const TrackChoice& track : tracks) {
        std::vector<std::u16string>& row = racerNames.emplace_back();
        for (const RacerChoice& racer : track.racers) {
            const auto index = static_cast<std::size_t>(racer.nameIndex);
            row.push_back(racer.nameIndex >= 0 && index < names.size() ? names[index]
                                                                       : Widen(racer.code));
        }
    }
    return racerNames;
}

}

Result<std::unique_ptr<SingleRaceScene>> SingleRaceScene::Create(render::Renderer& renderer,
                                                                 const assets::Vfs& vfs) {
    Result<MenuFiles> files = ReadMenuFiles(vfs, kScreen);
    Result<MenuPage> page = files.and_then([&](const MenuFiles& read) {
        return BuildMenuPage(renderer, vfs, read, std::span<const ButtonLine>{});
    });
    Result<std::vector<TrackChoice>> tracks =
        page.and_then([&](const MenuPage&) { return LoadTrackChoices(vfs); });
    if (!tracks) {
        return std::unexpected(std::move(tracks.error()));
    }
    return std::unique_ptr<SingleRaceScene>(
        new SingleRaceScene(renderer, vfs, std::move(page->pictures), TextButtonStates(*files),
                            SingleRaceMenu(std::move(*tracks))));
}

void SingleRaceScene::ApplySettings(const settings::Registry& settings) {
    rules_ = {.respawn = RespawnRuleFrom(settings), .aiDriving = AiDrivingFrom(settings)};
    if (!text_.Apply(*renderer_, *vfs_, settings)) {
        return;
    }
    const Result<std::vector<std::u16string>> names = ReadDriverNames(*vfs_, text_.Language());
    if (!names) {
        log::Warn("racers show their codes: {}", names.error().message);
    }
    racerNames_ = RacerNames(menu_.Tracks(), names.value_or(std::vector<std::u16string>{}));
}

std::optional<std::string> SingleRaceScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

void SingleRaceScene::Tick(const sim::InputFrame& input) {
    if (std::optional<std::string> next = menu_.Tick(input, rules_)) {
        next_ = std::move(next);
    }
}

void SingleRaceScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = menu_.StateHash();
}

void SingleRaceScene::Render([[maybe_unused]] const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    DrawPictures(pictures_, sprites_);
    DrawLabel(text_, {.font = std::string(kTitleFont)}, text_.Line(kTitleLine), kLeft, kTitleTop,
              sprites_);
    const std::size_t track = menu_.TrackIndex();
    DrawRow(SingleRaceRow::Track, u"TRACK", trackNames_.at(track));
    DrawRow(SingleRaceRow::Racer, u"RACER",
            racerNames_.empty() ? std::u16string_view{} : racerNames_.at(track).at(menu_.Racer()));
    DrawRow(SingleRaceRow::Race, text_.Line(kRaceLine), {});
    DrawRow(SingleRaceRow::Back, text_.Line(kBackLine), {});
    Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

SingleRaceScene::SingleRaceScene(render::Renderer& renderer, const assets::Vfs& vfs,
                                 std::vector<MenuPicture> pictures, ButtonStates states,
                                 SingleRaceMenu menu)
    : renderer_(&renderer),
      vfs_(&vfs),
      pictures_(std::move(pictures)),
      states_(std::move(states)),
      menu_(std::move(menu)) {
    for (const TrackChoice& track : menu_.Tracks()) {
        trackNames_.push_back(Widen(track.track));
    }
}

void SingleRaceScene::DrawRow(SingleRaceRow row, std::u16string_view label,
                              std::u16string_view value) {
    const MenuButtonState& state = states_.at(menu_.Focus() == row ? kFocusedState : kNormalState);
    const float top = kRowTops.at(static_cast<std::size_t>(row));
    DrawLabel(text_, state, label, kLeft, top, sprites_);
    DrawLabel(text_, state, value, kValueLeft, top, sprites_);
}

}
