#include "app/intro/intro_videos.hpp"

#include <charconv>
#include <format>
#include <system_error>

#include "app/cutscene/cutscene_screen.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kIntroVideoPrefix = "intro:";

}

std::string IntroVideoSceneName(std::size_t video) {
    return std::format("{}{}", kIntroVideoPrefix, video);
}

std::optional<std::size_t> ParseIntroVideoScene(std::string_view scene) {
    if (!scene.starts_with(kIntroVideoPrefix)) {
        return std::nullopt;
    }
    const std::string_view number = scene.substr(kIntroVideoPrefix.size());
    std::size_t video = 0;
    const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), video);
    if (error != std::errc() || end != number.data() + number.size() ||
        video >= kIntroVideos.size()) {
        return std::nullopt;
    }
    return video;
}

std::string SceneAfterIntroVideo(std::size_t video) {
    return video + 1 < kIntroVideos.size()
               ? IntroVideoSceneName(video + 1)
               : CutsceneScreenName({.menu = std::string(kLegalScreen)});
}

std::string FirstStartupScene(bool playsIntroVideos) {
    return playsIntroVideos ? IntroVideoSceneName(0)
                            : CutsceneScreenName({.menu = std::string(kLegalScreen)});
}

}
