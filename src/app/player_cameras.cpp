#include "app/player_cameras.hpp"

#include "app/frame_loop.hpp"

namespace rr::app {

render::Viewport PlayerViewport(std::size_t player, std::size_t players) {
    const float height = 1.0F / static_cast<float>(players);
    return {.y = height * static_cast<float>(player), .height = height};
}

render::Camera PlayerLens(render::Camera camera, std::size_t players) {
    if (players > 1) {
        camera.verticalFovDegrees -= kSplitFovCutDegrees;
    }
    return camera;
}

const Transform& PlayerCameraPose(const sim::RenderSnapshot& snapshot, std::size_t player) {
    return player == 0 ? snapshot.camera.transform
                       : snapshot.otherPlayerCameras.at(player - 1).transform;
}

PlayerCameras::PlayerCameras(const sim::RaceSession& session, std::size_t players) {
    for (std::size_t slot = 0; slot < players; ++slot) {
        RaceCamera& camera = cameras_.emplace_back(SubjectOf(session.Racers()[slot]));
        if (players > 1) {
            camera.SetView(CameraView::Standard, true);
        }
        poses_.push_back(camera.Pose());
    }
}

void PlayerCameras::CycleView(std::size_t player, const sim::RaceSession& session) {
    const bool hasFinished = session.Progress().OfRacer(player).finishPlace.has_value();
    if (session.Clock().Phase() != sim::RacePhase::Intro && !hasFinished) {
        cameras_.at(player).CycleView();
    }
}

void PlayerCameras::SetLookBack(std::size_t player, bool isLookingBack) {
    cameras_.at(player).SetLookBack(isLookingBack);
}

void PlayerCameras::Follow(const sim::RaceSession& session) {
    const bool isSplitScreen = cameras_.size() > 1;
    for (std::size_t slot = 0; slot < cameras_.size(); ++slot) {
        const sim::Racer& racer = session.Racers()[slot];
        RaceCamera& camera = cameras_[slot];
        if (session.WasRespawned(slot) || racer.Events().hasWarpLanded) {
            camera.Cut();
        }
        const bool hasFinished = session.Progress().OfRacer(slot).finishPlace.has_value();
        if (hasFinished && camera.View() != CameraView::Finish && !racer.Status().isGhost &&
            !racer.Status().isWarping) {
            camera.SetView(CameraView::Finish, isSplitScreen);
        }
        camera.Follow(SubjectOf(racer), static_cast<float>(kSimTick.count()));
        poses_[slot] = camera.Pose();
    }
}

void PlayerCameras::Snapshot(sim::RenderSnapshot& out) const {
    out.camera = {.transform = poses_.front(), .flags = out.entities.front().flags};
    out.otherPlayerCameras.clear();
    for (std::size_t slot = 1; slot < poses_.size(); ++slot) {
        out.otherPlayerCameras.push_back(
            {.transform = poses_[slot], .flags = out.entities[slot].flags});
    }
}

}
