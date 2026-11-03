#pragma once

struct ImDrawData;

namespace rr::render {

/// Drawn over the finished frame at full window resolution, without depth.
class OverlayPass {
public:
    virtual ~OverlayPass() = default;

    /// Null when nothing was built since the last call. Valid until the next build.
    [[nodiscard]] virtual ImDrawData* TakeDrawData() = 0;
};

}
