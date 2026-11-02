#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "app/scene.hpp"
#include "audio/music_player.hpp"

namespace rr::app {

/// Plays each top scene's music by its `MusicStart`. Leaving a `Follow` scene stops its music.
class SceneMusicFollower {
public:
    /// `sceneChanges` is the scene stack's `ChangeCount`, read every frame.
    void Update(audio::MusicPlayer& music, const SceneMusic& wanted, std::uint64_t sceneChanges);

private:
    std::optional<std::uint64_t> sceneChanges_;
    std::optional<std::string> asked_;
    MusicStart start_ = MusicStart::Follow;
};

}
