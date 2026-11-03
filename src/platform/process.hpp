#pragma once

#include <span>
#include <string>

#include "core/result.hpp"

namespace rr::platform {

/// `command` is the program then its arguments. The new process outlives this one.
[[nodiscard]] Result<void> LaunchDetached(std::span<const std::string> command);

}
