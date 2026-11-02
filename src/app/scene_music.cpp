#include "app/scene_music.hpp"

#include "core/log.hpp"

namespace rr::app {
namespace {

void Play(audio::MusicPlayer& music, const SceneMusic& wanted) {
    if (wanted.track.empty()) {
        music.Stop();
        return;
    }
    const Result<void> played = music.Play(wanted.track, wanted.repeat);
    if (!played) {
        log::Warn("{}", played.error().message);
    }
}

}

void SceneMusicFollower::Update(audio::MusicPlayer& music, const SceneMusic& wanted,
                                std::uint64_t sceneChanges) {
    const bool entered = sceneChanges_ != sceneChanges;
    if (entered) {
        if (start_ == MusicStart::Follow && wanted.start != MusicStart::Follow) {
            music.Stop();
        }
        sceneChanges_ = sceneChanges;
        asked_.reset();
        start_ = wanted.start;
    }
    switch (wanted.start) {
        case MusicStart::Follow:
            if (asked_ != wanted.track) {
                asked_ = wanted.track;
                Play(music, wanted);
            }
            return;
        case MusicStart::OnEntry:
            if (entered) {
                Play(music, wanted);
            }
            return;
        case MusicStart::OnEntryIfSilent:
            if (entered && !music.IsPlaying()) {
                Play(music, wanted);
            }
            return;
        case MusicStart::Keep:
            return;
    }
}

}
