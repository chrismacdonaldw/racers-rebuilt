#include "app/menu/workshop/track_backdrops.hpp"

#include <chrono>
#include <utility>

#include "core/log.hpp"

namespace rr::app {

TrackBackdrops::TrackBackdrops(render::Renderer& renderer, const assets::Vfs& vfs,
                               const Vector3& spot)
    : renderer_(&renderer), vfs_(&vfs), spot_(spot) {}

void TrackBackdrops::Want(std::string_view track) {
    wanted_ = track;
}

void TrackBackdrops::Tick() {
    if (reading_ &&
        reading_->files.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        Finish(*reading_);
        reading_.reset();
    }
    const bool isKnown = wanted_.empty() || loaded_.contains(wanted_) || failed_.contains(wanted_);
    if (reading_ || isKnown) {
        return;
    }
    reading_ = Reading{.track = wanted_,
                       .start = std::chrono::steady_clock::now(),
                       .files = std::async(std::launch::async, [vfs = vfs_, track = wanted_] {
                           return ReadBackdropFiles(*vfs, track);
                       })};
}

TrackBackdrop* TrackBackdrops::Shown() const {
    const auto found = loaded_.find(wanted_);
    return found != loaded_.end() ? found->second.get() : nullptr;
}

void TrackBackdrops::Finish(Reading& reading) {
    Result<BackdropFiles> files = reading.files.get();
    Result<std::unique_ptr<TrackBackdrop>> backdrop = files.and_then([&](BackdropFiles& read) {
        return TrackBackdrop::Upload(*renderer_, std::move(read), spot_);
    });
    if (!backdrop) {
        log::Warn("the workshop shows its colour, not {}: {}", reading.track,
                  backdrop.error().message);
        failed_.insert(reading.track);
        return;
    }
    const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - reading.start);
    log::Info("the workshop loaded {} in {} ms", reading.track, took.count());
    loaded_.emplace(std::move(reading.track), std::move(*backdrop));
}

}
