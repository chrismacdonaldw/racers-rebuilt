#pragma once

#include "sim/input_frame.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

class Scene {
public:
    virtual ~Scene() = default;

    virtual void Enter() {}
    virtual void Exit() {}

    /// Advances exactly one `kSimTick`. Must not read clocks, settings or files.
    virtual void Tick(const sim::InputFrame& input) = 0;

    /// Overwrites `out`, keeping its storage.
    virtual void Snapshot(sim::RenderSnapshot& out) const = 0;

    virtual void Render(const sim::RenderSnapshot& interpolated) = 0;
};

}
