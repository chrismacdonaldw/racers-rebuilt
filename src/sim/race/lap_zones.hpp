#pragma once

#include <array>
#include <cstdint>

#include "assets/event_table.hpp"

namespace rr::sim {

/// One racer's memory of the lap zones it passed, by the 1999 history or the 2001 single state.
class LapZones {
public:
    explicit LapZones(bool keepsOneZone = false);

    /// True when this entry is a finish line crossing that completes a lap.
    [[nodiscard]] bool Enter(assets::LapZone zone);

    /// After a backward crossing of the start checkpoint.
    void Reverse();

private:
    enum class Kind : std::uint8_t { Open, Finish, PostLine, Blocked };

    [[nodiscard]] bool EnterHistory(Kind kind);
    [[nodiscard]] bool EnterState(Kind kind);

    bool keepsOneZone_ = false;
    std::array<Kind, 3> history_;
};

}
