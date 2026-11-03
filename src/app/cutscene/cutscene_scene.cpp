#include "app/cutscene/cutscene_scene.hpp"

#include <format>
#include <utility>

#include "app/cutscene/cutscene_screen.hpp"
#include "app/frame_loop.hpp"
#include "app/video_settings.hpp"
#include "assets/game_path.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kSkipAfter{1000};

}

bool IsCutsceneScene(std::string_view scene) {
    return scene.starts_with(kCutsceneScenePrefix) || IsCutsceneScreen(scene);
}

Result<CutscenePlay> ViewerPlay(const assets::Vfs& vfs, std::string_view scene,
                                const std::optional<settings::Racer>& playerRacer) {
    const std::string_view path = scene.substr(kCutsceneScenePrefix.size());
    const std::size_t slash = path.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 == path.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} isn't {}<folder>/<file>", scene, kCutsceneScenePrefix));
    }
    const std::string_view folder = path.substr(0, slash);
    CutscenePlay play{.folder = std::format("MENUDATA/{}", assets::ToUpperAscii(folder)),
                      .file = assets::ToUpperAscii(path.substr(slash + 1))};
    if (const std::optional<CutsceneScreen> screen = FindCutsceneScreen(folder)) {
        CastScreenRacer(vfs, *screen, "", playerRacer, play);
    }
    return play;
}

Result<std::unique_ptr<CutsceneScene>> CutsceneScene::Create(
    const RaceOutputs& outputs, const assets::Vfs& vfs, assets::TrackLoader& loader,
    std::string_view language, std::string_view scene, const AwardCast& cast) {
    return (IsCutsceneScreen(scene) ? ScreenPlay(vfs, scene, cast.playerRacer)
                                    : ViewerPlay(vfs, scene, cast.playerRacer))
        .and_then([&](CutscenePlay play) {
            play.carBody = cast.carBody;
            return CutsceneView::Create(outputs, vfs, loader, language, std::move(play));
        })
        .transform([&](std::unique_ptr<CutsceneView> view) {
            return std::unique_ptr<CutsceneScene>(
                new CutsceneScene(*outputs.renderer, std::move(view)));
        });
}

void CutsceneScene::ApplySettings(const settings::Registry& settings) {
    view_->DrawFarScenery(settings.Get(kFarScenerySetting));
}

std::optional<std::string> CutsceneScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

void CutsceneScene::HandleEvent(const SDL_Event& event) {
    isSkipAsked_ = isSkipAsked_ || IsSkipPress(event);
}

void CutsceneScene::Tick([[maybe_unused]] const sim::InputFrame& input) {
    const bool isSkipped =
        std::exchange(isSkipAsked_, false) && view_->Play().isSkippable && played_ >= kSkipAfter;
    if (hasEnded_) {
        return;
    }
    if (isSkipped) {
        End();
        return;
    }
    view_->Advance(kSimTick);
    played_ += kSimTick;
    if (!view_->Play().isLooping && view_->IsComplete()) {
        End();
    }
}

void CutsceneScene::End() {
    hasEnded_ = true;
    if (!view_->Play().next.empty()) {
        next_ = view_->Play().next;
    }
}

void CutsceneScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entityClocks.assign(view_->Clocks().begin(), view_->Clocks().end());
}

void CutsceneScene::Render(const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    render::RenderView view = view_->Render(interpolated.entityClocks, sprites_);
    view.sprites = sprites_;
    Result<void> drawn = renderer_->Draw(view);
    if (!drawn) {
        log::Error("{}/{}: {}", view_->Play().folder, view_->Play().file, drawn.error().message);
    }
}

}
