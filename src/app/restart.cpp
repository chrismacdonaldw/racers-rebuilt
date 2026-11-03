#include "app/restart.hpp"

#include <algorithm>
#include <array>

#include "core/log.hpp"
#include "platform/process.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kModsCategory = "Mods";
constexpr std::array kOneShotFlags{std::string_view("--record"), std::string_view("--replay"),
                                   std::string_view("--import-save"),
                                   std::string_view("--dump-images")};

bool IsOneShotWithValue(std::string_view arg) {
    return std::ranges::any_of(kOneShotFlags, [&](std::string_view flag) {
        return arg.size() > flag.size() && arg.starts_with(flag) && arg[flag.size()] == '=';
    });
}

}

std::vector<std::string> RelaunchCommand(std::span<const std::string_view> argv) {
    std::vector<std::string> command;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i > 0 && std::ranges::contains(kOneShotFlags, argv[i])) {
            ++i;
            continue;
        }
        if (i > 0 && IsOneShotWithValue(argv[i])) {
            continue;
        }
        command.emplace_back(argv[i]);
    }
    return command;
}

ModChanges::ModChanges(const settings::Registry& registry) {
    for (const auto& [key, declaration] : registry.AllDeclarations()) {
        if (declaration.category == kModsCategory) {
            startup_.emplace(key, registry.AllValues().find(key)->second);
        }
    }
}

std::optional<overlay::MenuChoice> Restart::Choice(const settings::Registry& registry,
                                                   bool& isQuitting) const {
    if (command_.empty() || !changes_.Any(registry)) {
        return std::nullopt;
    }
    return overlay::MenuChoice{.name = "Restart now", .choose = [this, &isQuitting] {
                                   const Result<void> launched = platform::LaunchDetached(command_);
                                   if (!launched) {
                                       log::Error("{}", launched.error().message);
                                       return;
                                   }
                                   isQuitting = true;
                               }};
}

std::optional<overlay::MenuChoice> LiveRemount::Choice(const settings::Registry& registry) {
    if (!isAllowed_ || !changes_.Any(registry)) {
        return std::nullopt;
    }
    return overlay::MenuChoice{.name = "Apply now", .choose = [this] { isRequested_ = true; }};
}

std::vector<overlay::MenuChoice> ModActions(LiveRemount& remount, const Restart& restart,
                                            const settings::Registry& registry, bool& isQuitting) {
    std::vector<overlay::MenuChoice> actions;
    std::array choices{remount.Choice(registry), restart.Choice(registry, isQuitting)};
    for (std::optional<overlay::MenuChoice>& action : choices) {
        if (action) {
            actions.push_back(std::move(*action));
        }
    }
    return actions;
}

bool ModChanges::Any(const settings::Registry& registry) const {
    return std::ranges::any_of(startup_, [&](const auto& entry) {
        return registry.AllValues().find(entry.first)->second != entry.second;
    });
}

}
