#include "mods/mod_toggles.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <optional>
#include <utility>

#include "mods/mod_resolution.hpp"

namespace rr::mods {
namespace {

std::string JoinNames(std::span<const Mod> mods, std::span<const std::size_t> indices) {
    std::string names;
    for (const std::size_t index : indices) {
        names += std::format("{}{}", names.empty() ? "" : ", ", mods[index].Manifest().name);
    }
    return names;
}

std::vector<ModSwitch> Switches(std::span<const std::size_t> indices, bool isOn) {
    std::vector<ModSwitch> switches;
    for (const std::size_t index : indices) {
        switches.push_back({.index = index, .isOn = isOn});
    }
    return switches;
}

std::vector<std::size_t> RequiredButOff(std::span<const Mod> mods, const std::vector<bool>& isOn,
                                        std::vector<std::size_t> pending) {
    std::vector<bool> isVisited(mods.size(), false);
    for (const std::size_t index : pending) {
        isVisited[index] = true;
    }
    std::vector<std::size_t> off;
    while (!pending.empty()) {
        const std::size_t at = pending.back();
        pending.pop_back();
        for (const ModReference& reference : mods[at].Manifest().requirements) {
            const std::optional<std::size_t> found = FindMod(mods, reference.id);
            if (!found || isVisited[*found] ||
                !Matches(reference.range, mods[*found].Manifest().version)) {
                continue;
            }
            isVisited[*found] = true;
            pending.push_back(*found);
            if (!isOn[*found]) {
                off.push_back(*found);
            }
        }
    }
    return off;
}

std::vector<std::size_t> Dependents(std::span<const Mod> mods, const std::vector<bool>& isOn,
                                    std::span<const std::size_t> removed) {
    std::vector<bool> isRemoved(mods.size(), false);
    for (const std::size_t index : removed) {
        isRemoved[index] = true;
    }
    const auto needsRemoved = [&](const ModReference& reference) {
        const std::optional<std::size_t> found = FindMod(mods, reference.id);
        return found && isRemoved[*found];
    };
    std::vector<std::size_t> dependents;
    for (bool isGrowing = true; isGrowing;) {
        isGrowing = false;
        for (std::size_t index = 0; index < mods.size(); ++index) {
            if (isOn[index] && !isRemoved[index] &&
                std::ranges::any_of(mods[index].Manifest().requirements, needsRemoved)) {
                isRemoved[index] = true;
                dependents.push_back(index);
                isGrowing = true;
            }
        }
    }
    std::ranges::sort(dependents);
    return dependents;
}

ModToggle PlanTurnOff(std::span<const Mod> mods, const std::vector<bool>& isOn, std::size_t index) {
    const std::vector<std::size_t> dependents = Dependents(mods, isOn, std::span(&index, 1));
    std::vector<ModSwitch> switches = Switches(dependents, false);
    switches.push_back({.index = index, .isOn = false});
    if (dependents.empty()) {
        return {.question = {}, .choices = {{.label = {}, .switches = std::move(switches)}}};
    }
    return {.question = std::format("Turning off {} also turns off {}.",
                                    mods[index].Manifest().name, JoinNames(mods, dependents)),
            .choices = {{.label = "Turn them off", .switches = std::move(switches)},
                        {.label = "Cancel", .switches = {}}}};
}

ModToggle PlanTurnOn(std::span<const Mod> mods, const std::vector<bool>& isOn, std::size_t index) {
    std::vector<std::size_t> turnedOn = RequiredButOff(mods, isOn, {index});
    turnedOn.insert(turnedOn.begin(), index);
    std::vector<ModSwitch> switches = Switches(turnedOn, true);
    std::vector<std::size_t> conflicting;
    for (std::size_t other = 0; other < mods.size(); ++other) {
        if (isOn[other] && std::ranges::any_of(turnedOn, [&](std::size_t on) {
                return AreInConflict(mods[other], mods[on]);
            })) {
            conflicting.push_back(other);
        }
    }
    if (conflicting.empty()) {
        return {.question = {}, .choices = {{.label = {}, .switches = std::move(switches)}}};
    }
    std::ranges::move(Switches(conflicting, false), std::back_inserter(switches));
    std::ranges::move(Switches(Dependents(mods, isOn, conflicting), false),
                      std::back_inserter(switches));
    const std::string name = mods[index].Manifest().name;
    const std::string others = JoinNames(mods, conflicting);
    return {.question = std::format("{} conflicts with {}.", name, others),
            .choices = {{.label = std::format("Keep {}", name), .switches = std::move(switches)},
                        {.label = std::format("Keep {}", others), .switches = {}}}};
}

}

ModToggle PlanModToggle(std::span<const Mod> mods, const std::vector<bool>& isOn,
                        std::size_t index) {
    return isOn[index] ? PlanTurnOff(mods, isOn, index) : PlanTurnOn(mods, isOn, index);
}

std::vector<ModSwitch> RequiredSwitches(std::span<const Mod> mods, const std::vector<bool>& isOn) {
    std::vector<std::size_t> on;
    for (std::size_t index = 0; index < mods.size(); ++index) {
        if (isOn[index]) {
            on.push_back(index);
        }
    }
    return Switches(RequiredButOff(mods, isOn, std::move(on)), true);
}

}
