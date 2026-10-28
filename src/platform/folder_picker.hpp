#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "core/result.hpp"

namespace rr::platform {

/// Shows a folder dialog and waits for it while pumping events. Returns nothing if the user
/// cancels. Requires a live `SdlContext`. Main thread only.
[[nodiscard]] Result<std::optional<std::filesystem::path>> PickFolder(const std::string& title);

}
