#include "app/cutscene/cutscene_screen.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <utility>
#include <variant>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_scenes.hpp"
#include "assets/game_path.hpp"
#include "assets/menu_layout.hpp"

namespace rr::app {
namespace {

constexpr char kNextSeparator = '>';
constexpr std::string_view kCircuitPrefix = "c";
constexpr std::string_view kIntroPrefix = "circuit";
constexpr std::size_t kCircuitCount = 7;

constexpr std::array kScreens{
    CutsceneScreen{.menu = kLegalScreen, .layout = "SPLASH", .next = kMainMenuScene},
    CutsceneScreen{.menu = kCreditsScreen,
                   .layout = "SPLASH",
                   .music = "credits",
                   .loopsMusic = true,
                   .next = kMainMenuScene},
    CutsceneScreen{.menu = "circuit1", .layout = "CIRCUIT1", .music = "circuit1"},
    CutsceneScreen{.menu = "circuit2", .layout = "CIRCUIT2", .music = "circuit2"},
    CutsceneScreen{.menu = "circuit3", .layout = "CIRCUIT3", .music = "circuit3"},
    CutsceneScreen{.menu = "circuit4", .layout = "CIRCUIT4", .music = "circuit4"},
    CutsceneScreen{.menu = "circuit5", .layout = "CIRCUIT5", .music = "circuit5"},
    CutsceneScreen{.menu = "circuit6", .layout = "CIRCUIT6", .music = "circuit6"},
    CutsceneScreen{.menu = "circuit7", .layout = "CIRCUIT7", .music = "rracer"},
    CutsceneScreen{.menu = "c_award1",
                   .layout = "C_AWARD1",
                   .music = "1st",
                   .isSkippable = false,
                   .next = kMainMenuScene},
    CutsceneScreen{.menu = "c_award2",
                   .layout = "C_AWARD2",
                   .music = "2nd",
                   .isSkippable = false,
                   .next = kMainMenuScene},
    CutsceneScreen{.menu = "c_award3",
                   .layout = "C_AWARD3",
                   .music = "3rd",
                   .isSkippable = false,
                   .next = kMainMenuScene},
    CutsceneScreen{.menu = "c_award4",
                   .layout = "C_AWARD4",
                   .music = "lose",
                   .isSkippable = false,
                   .next = kMainMenuScene},
    CutsceneScreen{.menu = kWinCarScreen,
                   .layout = "WINCAR",
                   .music = "gencar",
                   .isSkippable = false,
                   .next = kMainMenuScene},
    CutsceneScreen{.menu = kWinRocketRacerCarScreen,
                   .layout = "WINRRCAR",
                   .music = "rrcar",
                   .isSkippable = false,
                   .next = "screen:credits"},
    CutsceneScreen{.menu = kWinRecordsCarScreen,
                   .layout = "WINVVCAR",
                   .music = "vvcar",
                   .isSkippable = false,
                   .next = kMainMenuScene},
};

Result<const assets::MenuElement*> FindElement(const assets::MenuLayout& layout,
                                               const CutsceneScreen& screen) {
    const assets::MenuElement* found = layout.Find(screen.menu);
    const auto* element =
        found != nullptr ? std::get_if<assets::CutsceneElement>(&found->body) : nullptr;
    if (element == nullptr) {
        return Fail(ErrorCode::NotFound,
                    std::format("{}.MIB has no cutscene {}", screen.layout, screen.menu));
    }
    if (element->playback && *element->playback != std::array<std::int32_t, 2>{}) {
        return Fail(
            ErrorCode::InvalidData,
            std::format("cutscene {} picks a frame, which no retail screen does", screen.menu));
    }
    return found;
}

}

std::string CutsceneScreenName(const CutsceneScreenSetup& setup) {
    return setup.next.empty() ? std::format("{}{}", kCutsceneScreenPrefix, setup.menu)
                              : std::format("{}{}{}{}", kCutsceneScreenPrefix, setup.menu,
                                            kNextSeparator, setup.next);
}

bool IsCutsceneScreen(std::string_view scene) {
    return scene.starts_with(kCutsceneScreenPrefix);
}

Result<CutsceneScreenSetup> ParseCutsceneScreen(std::string_view scene) {
    const std::string_view rest = scene.substr(kCutsceneScreenPrefix.size());
    const std::size_t separator = rest.find(kNextSeparator);
    CutsceneScreenSetup setup{.menu = std::string(rest.substr(0, separator))};
    if (separator != std::string_view::npos) {
        setup.next = std::string(rest.substr(separator + 1));
    }
    const std::optional<CutsceneScreen> screen = FindCutsceneScreen(setup.menu);
    if (!screen) {
        return Fail(ErrorCode::InvalidData, std::format("{} isn't a cutscene screen", setup.menu));
    }
    if (setup.next.empty() && screen->next.empty()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} needs a scene to show next, as in {}{}{}<scene>", setup.menu,
                                kCutsceneScreenPrefix, setup.menu, kNextSeparator));
    }
    return setup;
}

std::optional<CutsceneScreen> FindCutsceneScreen(std::string_view menu) {
    const auto found = std::ranges::find(kScreens, menu, &CutsceneScreen::menu);
    return found == kScreens.end() ? std::nullopt : std::optional(*found);
}

std::optional<std::string> CircuitIntro(std::string_view circuit) {
    if (!circuit.starts_with(kCircuitPrefix)) {
        return std::nullopt;
    }
    const std::string_view digits = circuit.substr(kCircuitPrefix.size());
    std::size_t index = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), index);
    if (error != std::errc{} || end != digits.data() + digits.size() || index >= kCircuitCount) {
        return std::nullopt;
    }
    return std::format("{}{}", kIntroPrefix, index + 1);
}

Result<CutsceneScreenPlacement> ReadCutsceneScreen(const assets::Vfs& vfs,
                                                   const CutsceneScreen& screen) {
    const std::string file =
        assets::JoinGamePath(kMenuFolder, std::format("{}.MIB", screen.layout));
    Result<assets::MenuLayout> layout = vfs.Read(file).and_then(
        [](const assets::FileData& data) { return assets::ParseMenuLayout(data.Bytes()); });
    if (!layout) {
        return Fail(layout.error().code, std::format("{}: {}", file, layout.error().message));
    }
    return FindElement(*layout, screen).transform([&](const assets::MenuElement* found) {
        const assets::MenuRect& rect = found->widget.rect;
        return CutsceneScreenPlacement{
            .folder = assets::JoinGamePath(kMenuFolder, assets::ToUpperAscii(screen.menu)),
            .cutscene =
                assets::ToUpperAscii(std::get<assets::CutsceneElement>(found->body).cutscene),
            .rect = {.x = static_cast<float>(rect.left),
                     .y = static_cast<float>(rect.top),
                     .width = static_cast<float>(rect.right - rect.left),
                     .height = static_cast<float>(rect.bottom - rect.top)}};
    });
}

bool IsSkipPress(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            return !event.key.repeat;
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
        case SDL_EVENT_JOYSTICK_BUTTON_UP:
            return true;
        default:
            return false;
    }
}

Result<CutscenePlay> ScreenPlay(const assets::Vfs& vfs, std::string_view scene) {
    Result<CutsceneScreenSetup> setup = ParseCutsceneScreen(scene);
    if (!setup) {
        return std::unexpected(std::move(setup.error()));
    }
    const CutsceneScreen screen = FindCutsceneScreen(setup->menu).value_or(CutsceneScreen{});
    return ReadCutsceneScreen(vfs, screen).transform([&](CutsceneScreenPlacement placed) {
        return CutscenePlay{
            .folder = std::move(placed.folder),
            .file = std::move(placed.cutscene),
            .rect = placed.rect,
            .isLooping = false,
            .isSkippable = screen.isSkippable,
            .music = {.track = screen.music,
                      .repeat = screen.loopsMusic ? audio::Repeat::Loop : audio::Repeat::Once},
            .next = setup->next.empty() ? std::string(screen.next) : std::move(setup->next)};
    });
}

}
