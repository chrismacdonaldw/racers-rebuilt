#include "app/cutscene/cutscene_effect_player.hpp"

#include <utility>

namespace rr::app {

CutsceneEffectPlayer::CutsceneEffectPlayer(const assets::CutsceneFrame& frame,
                                           const assets::CutsceneEffectsAsset& effects,
                                           CutsceneEffects parts)
    : frame_(&frame), effects_(&effects), parts_(std::move(parts)) {}

void CutsceneEffectPlayer::Advance(std::span<const CutsceneEventChange> changes,
                                   Milliseconds elapsed) {
    commands_.clear();
    DispatchLinks(*frame_, effects_->links, changes, commands_);
    for (const EffectCommand& command : commands_) {
        Run(command);
    }
    parts_.particles.Update(elapsed);
    parts_.overlay.Advance(elapsed);
}

void CutsceneEffectPlayer::StopAll() {
    parts_.tints.StopAll();
    parts_.sounds.StopAll();
    for (std::size_t text = 0; text < effects_->texts.size(); ++text) {
        parts_.overlay.Stop(assets::EffectKind::Text, text);
    }
    for (std::size_t image = 0; image < effects_->images.size(); ++image) {
        parts_.overlay.Stop(assets::EffectKind::Image, image);
    }
}

void CutsceneEffectPlayer::SetFrame(const assets::CutsceneFrame& frame) {
    StopAll();
    frame_ = &frame;
}

void CutsceneEffectPlayer::Run(const EffectCommand& command) {
    switch (command.kind) {
        case assets::EffectKind::Tint:
            if (command.starts) {
                parts_.tints.Start(command.effect);
            } else {
                parts_.tints.Stop(command.effect);
            }
            return;
        case assets::EffectKind::Sound:
            if (command.starts) {
                parts_.sounds.Start(command.effect);
            } else {
                parts_.sounds.Stop(command.effect);
            }
            return;
        case assets::EffectKind::Particle:
            if (command.starts) {
                parts_.particles.Start(command.effect, command.position, command.axes);
            } else {
                parts_.particles.Stop(command.effect);
            }
            return;
        case assets::EffectKind::Fade:
        case assets::EffectKind::Text:
        case assets::EffectKind::Image:
            if (command.starts) {
                parts_.overlay.Start(command.kind, command.effect);
            } else {
                parts_.overlay.Stop(command.kind, command.effect);
            }
            return;
    }
}

}
