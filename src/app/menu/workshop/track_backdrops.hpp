#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "app/menu/workshop/track_backdrop.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// Main thread only. Reads one track at a time on a worker. `renderer` and `vfs` must outlive it.
class TrackBackdrops {
public:
    /// `spot` is where each track's first grid spot goes, in game space.
    TrackBackdrops(render::Renderer& renderer, const assets::Vfs& vfs, const Vector3& spot);

    TrackBackdrops(const TrackBackdrops&) = delete;
    TrackBackdrops& operator=(const TrackBackdrops&) = delete;
    TrackBackdrops(TrackBackdrops&&) = delete;
    TrackBackdrops& operator=(TrackBackdrops&&) = delete;
    ~TrackBackdrops() = default;

    /// An empty `track` shows none.
    void Want(std::string_view track);

    void Tick();

    /// Null until the wanted track has loaded, or when it failed to.
    [[nodiscard]] TrackBackdrop* Shown() const;

private:
    struct Reading {
        std::string track;
        std::chrono::steady_clock::time_point start;
        std::future<Result<BackdropFiles>> files;
    };

    void Finish(Reading& reading);

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    Vector3 spot_;
    std::string wanted_;
    std::map<std::string, std::unique_ptr<TrackBackdrop>, std::less<>> loaded_;
    std::set<std::string, std::less<>> failed_;
    std::optional<Reading> reading_;
};

}
