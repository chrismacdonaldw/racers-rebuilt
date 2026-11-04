#pragma once

#include <string>
#include <vector>

#include "assets/vfs.hpp"

namespace rr::app {

/// What's wrong with the tracks, circuits and drivers that mods add, for `--check-mods`.
[[nodiscard]] std::vector<std::string> ModContentProblems(const assets::Vfs& vfs);

}
