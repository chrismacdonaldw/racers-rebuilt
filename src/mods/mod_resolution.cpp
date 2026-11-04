#include "mods/mod_resolution.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <utility>

namespace rr::mods {
namespace {

bool IsSatisfiedBy(const ModReference& reference, const Mod& mod) {
    return Matches(reference.range, mod.Manifest().version);
}

bool NamesInConflict(const Mod& from, const Mod& other) {
    return std::ranges::any_of(from.Manifest().conflicts, [&](const ModReference& reference) {
        return reference.id == other.Manifest().id && IsSatisfiedBy(reference, other);
    });
}

std::string WithRange(const ModReference& reference, std::string_view shown) {
    const std::string range = DescribeRange(reference.range);
    return range.empty() ? std::string(shown) : std::format("{} {}", shown, range);
}

class Resolver {
public:
    Resolver(std::span<const Mod> mods, const std::vector<bool>& isOn)
        : mods_(mods), isOn_(&isOn), active_(isOn) {}

    ModResolution Run() {
        while (HoldUnmetRequirements() || HoldConflicts() || HoldCycles()) {
        }
        std::ranges::sort(held_, {}, &HeldMod::index);
        return {.mountOrder = MountOrder(), .held = std::move(held_)};
    }

private:
    void Hold(std::size_t index, std::string reason) {
        active_[index] = false;
        held_.push_back({.index = index, .reason = std::move(reason)});
    }

    [[nodiscard]] std::optional<std::string> RequirementProblem(std::size_t index) const {
        for (const ModReference& reference : mods_[index].Manifest().requirements) {
            const std::optional<std::size_t> found = FindMod(mods_, reference.id);
            if (!found) {
                return std::format("Needs {}", WithRange(reference, reference.id));
            }
            const ModManifest& required = mods_[*found].Manifest();
            if (!IsSatisfiedBy(reference, mods_[*found])) {
                return std::format("Needs {}, has {}", WithRange(reference, required.name),
                                   required.version);
            }
            if (!(*isOn_)[*found]) {
                return std::format("Needs {}, which is off", required.name);
            }
            if (!active_[*found]) {
                return std::format("Needs {}, which is held back", required.name);
            }
        }
        return std::nullopt;
    }

    bool HoldUnmetRequirements() {
        bool isChanged = false;
        for (std::size_t index = 0; index < mods_.size(); ++index) {
            if (!active_[index]) {
                continue;
            }
            if (std::optional<std::string> problem = RequirementProblem(index)) {
                Hold(index, std::move(*problem));
                isChanged = true;
            }
        }
        return isChanged;
    }

    bool HoldConflicts() {
        for (std::size_t index = 0; index < mods_.size(); ++index) {
            for (std::size_t higher = 0; active_[index] && higher < index; ++higher) {
                if (active_[higher] && AreInConflict(mods_[index], mods_[higher])) {
                    Hold(index, std::format("Conflicts with {}", mods_[higher].Manifest().name));
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] std::vector<std::size_t> Prerequisites(std::size_t index) const {
        std::vector<std::size_t> below;
        const ModManifest& manifest = mods_[index].Manifest();
        for (const std::vector<ModReference>* references :
             {&manifest.requirements, &manifest.loadsAfter}) {
            for (const ModReference& reference : *references) {
                const std::optional<std::size_t> found = FindMod(mods_, reference.id);
                if (found && active_[*found] && IsSatisfiedBy(reference, mods_[*found])) {
                    below.push_back(*found);
                }
            }
        }
        return below;
    }

    [[nodiscard]] std::vector<std::size_t> MountOrder() const {
        std::vector<bool> isPlaced(mods_.size(), false);
        std::vector<std::size_t> order;
        for (bool isPlacing = true; isPlacing;) {
            isPlacing = false;
            for (std::size_t index = mods_.size(); index-- > 0;) {
                const auto isBelow = [&](std::size_t below) { return isPlaced[below]; };
                if (active_[index] && !isPlaced[index] &&
                    std::ranges::all_of(Prerequisites(index), isBelow)) {
                    isPlaced[index] = true;
                    order.push_back(index);
                    isPlacing = true;
                    break;
                }
            }
        }
        return order;
    }

    [[nodiscard]] bool ReachesItself(std::size_t start) const {
        std::vector<bool> isSeen(mods_.size(), false);
        std::vector<std::size_t> pending = Prerequisites(start);
        while (!pending.empty()) {
            const std::size_t at = pending.back();
            pending.pop_back();
            if (at == start) {
                return true;
            }
            if (!isSeen[at]) {
                isSeen[at] = true;
                std::ranges::copy(Prerequisites(at), std::back_inserter(pending));
            }
        }
        return false;
    }

    bool HoldCycles() {
        const std::vector<std::size_t> placed = MountOrder();
        std::vector<std::size_t> looped;
        for (std::size_t index = 0; index < mods_.size(); ++index) {
            if (active_[index] && !std::ranges::contains(placed, index) && ReachesItself(index)) {
                looped.push_back(index);
            }
        }
        for (const std::size_t index : looped) {
            std::string others;
            for (const std::size_t other : looped) {
                if (other != index) {
                    others += std::format("{}{}", others.empty() ? "" : ", ",
                                          mods_[other].Manifest().name);
                }
            }
            Hold(index, std::format("Its load order loops through {}", others));
        }
        return !looped.empty();
    }

    std::span<const Mod> mods_;
    const std::vector<bool>* isOn_;
    std::vector<bool> active_;
    std::vector<HeldMod> held_;
};

}

std::optional<std::size_t> FindMod(std::span<const Mod> mods, std::string_view id) {
    const auto found = std::ranges::find(
        mods, id, [](const Mod& mod) -> std::string_view { return mod.Manifest().id; });
    return found == mods.end() ? std::nullopt
                               : std::optional(static_cast<std::size_t>(found - mods.begin()));
}

bool AreInConflict(const Mod& a, const Mod& b) {
    return NamesInConflict(a, b) || NamesInConflict(b, a);
}

ModResolution ResolveMods(std::span<const Mod> mods, const std::vector<bool>& isOn) {
    return Resolver(mods, isOn).Run();
}

}
