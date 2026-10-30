#pragma once

#include <cstddef>
#include <string>

#include "sim/race/race_session.hpp"

namespace rr::app {

/// Replaces `out` with `racer`'s phase, lap, place and race time, keeping its storage.
void FormatRaceStatus(const sim::RaceSession& session, std::size_t racer, std::string& out);

}
