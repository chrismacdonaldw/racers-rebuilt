#pragma once

#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "overlay/overlay.hpp"
#include "settings/registry.hpp"

namespace rr::app {

/// Keeps the program and its arguments but drops one-shot work: recording, replays, imports, dumps.
[[nodiscard]] std::vector<std::string> RelaunchCommand(std::span<const std::string_view> argv);

/// Remembers the Mods settings at startup, which only take effect on a restart.
// NOLINTNEXTLINE(bugprone-exception-escape)
class ModChanges {
public:
    explicit ModChanges(const settings::Registry& registry);

    [[nodiscard]] bool Any(const settings::Registry& registry) const;

private:
    std::map<std::string, settings::SettingValue, std::less<>> startup_;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
class Restart {
public:
    Restart(std::vector<std::string> command, const settings::Registry& registry)
        : command_(std::move(command)), changes_(registry) {}

    /// Offered once a Mods setting changes. Choosing it relaunches and sets `isQuitting`.
    [[nodiscard]] std::optional<overlay::MenuChoice> Choice(const settings::Registry& registry,
                                                            bool& isQuitting) const;

    void Rebase(const settings::Registry& registry) { changes_ = ModChanges(registry); }

private:
    std::vector<std::string> command_;
    ModChanges changes_;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
class LiveRemount {
public:
    LiveRemount(bool isAllowed, const settings::Registry& registry)
        : isAllowed_(isAllowed), changes_(registry) {}

    /// Offered once a Mods setting changes, unless `isAllowed` was false. Choosing it only asks.
    [[nodiscard]] std::optional<overlay::MenuChoice> Choice(const settings::Registry& registry);

    [[nodiscard]] bool TakeRequest() { return std::exchange(isRequested_, false); }

    void Rebase(const settings::Registry& registry) { changes_ = ModChanges(registry); }

private:
    bool isAllowed_;
    bool isRequested_ = false;
    ModChanges changes_;
};

[[nodiscard]] std::vector<overlay::MenuChoice> ModActions(LiveRemount& remount,
                                                          const Restart& restart,
                                                          const settings::Registry& registry,
                                                          bool& isQuitting);

}
