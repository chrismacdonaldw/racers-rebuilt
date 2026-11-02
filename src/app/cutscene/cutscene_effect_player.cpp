#include "app/cutscene/cutscene_effect_player.hpp"

#include <utility>

namespace rr::app {

CutsceneEffectPlayer::CutsceneEffectPlayer(const assets::CutsceneFrame& frame,
                                           const assets::CutsceneEffectsAsset& effects,
                                           CutsceneSounds sounds, CutsceneOverlay overlay)
    : frame_(&frame),
      effects_(&effects),
      sounds_(std::move(sounds)),
      overlay_(std::move(overlay)) {}

void CutsceneEffectPlayer::Advance(std::span<const CutsceneEventChange> changes,
                                   Milliseconds elapsed) {
    commands_.clear();
    DispatchLinks(*frame_, effects_->links, changes, commands_);
    for (const EffectCommand& command : commands_) {
        if (command.kind == assets::EffectKind::Sound) {
            if (command.starts) {
                sounds_.Start(command.effect);
            } else {
                sounds_.Stop(command.effect);
            }
        } else if (command.starts) {
            overlay_.Start(command.kind, command.effect);
        } else {
            overlay_.Stop(command.kind, command.effect);
        }
    }
    overlay_.Advance(elapsed);
}

void CutsceneEffectPlayer::StopAll() {
    sounds_.StopAll();
    for (std::size_t text = 0; text < effects_->texts.size(); ++text) {
        overlay_.Stop(assets::EffectKind::Text, text);
    }
    for (std::size_t image = 0; image < effects_->images.size(); ++image) {
        overlay_.Stop(assets::EffectKind::Image, image);
    }
}

}
