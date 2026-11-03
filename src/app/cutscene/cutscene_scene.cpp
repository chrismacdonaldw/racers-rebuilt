#include "app/cutscene/cutscene_scene.hpp"

#include <format>
#include <utility>

#include "app/cutscene/cutscene_screen.hpp"
#include "app/frame_loop.hpp"
#include "assets/game_path.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kSkipAfter{1000};

}

bool IsCutsceneScene(std::string_view scene) {
    return scene.starts_with(kCutsceneScenePrefix) || IsCutsceneScreen(scene);
}

Result<CutscenePlay> ViewerPlay(std::string_view scene) {
    const std::string_view path = scene.substr(kCutsceneScenePrefix.size());
    const std::size_t slash = path.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 == path.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} isn't {}<folder>/<file>", scene, kCutsceneScenePrefix));
    }
    return CutscenePlay{
        .folder = std::format("MENUDATA/{}", assets::ToUpperAscii(path.substr(0, slash))),
        .file = assets::ToUpperAscii(path.substr(slash + 1))};
}

Result<std::unique_ptr<CutsceneScene>> CutsceneScene::Create(const RaceOutputs& outputs,
                                                             const assets::Vfs& vfs,
                                                             assets::TrackLoader& loader,
                                                             std::string_view language,
                                                             std::string_view scene) {
    return (IsCutsceneScreen(scene) ? ScreenPlay(vfs, scene) : ViewerPlay(scene))
        .and_then([&](CutscenePlay play) {
            return CutsceneView::Create(outputs, vfs, loader, language, std::move(play));
        })
        .transform([&](std::unique_ptr<CutsceneView> view) {
            return std::unique_ptr<CutsceneScene>(
                new CutsceneScene(*outputs.renderer, std::move(view)));
        });
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
