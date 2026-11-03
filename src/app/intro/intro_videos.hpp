#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace rr::app {

struct IntroVideo {
    std::string_view file;
    bool isSkippable = true;
};

inline constexpr std::array kIntroVideos{IntroVideo{.file = "lmicmp.avi", .isSkippable = false},
                                         IntroVideo{.file = "hvscmp.avi"},
                                         IntroVideo{.file = "introcmp.avi"}};

[[nodiscard]] std::string IntroVideoSceneName(std::size_t video);

/// The video's index in `kIntroVideos`.
[[nodiscard]] std::optional<std::size_t> ParseIntroVideoScene(std::string_view scene);

/// The next video, or the legal splash after the last.
[[nodiscard]] std::string SceneAfterIntroVideo(std::size_t video);

[[nodiscard]] std::string FirstStartupScene(bool playsIntroVideos);

}
