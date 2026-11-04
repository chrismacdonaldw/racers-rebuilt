#include "mods/mod_check.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <utility>

#include "core/log.hpp"
#include "core/utf8_path.hpp"

namespace rr::mods {

std::vector<std::string> ModProblems(const ModList& mods, const settings::Registry& registry) {
    std::vector<std::string> problems = mods.SkippedMods();
    for (const HeldMod& held : mods.Resolve(registry).held) {
        problems.push_back(std::format(
            "{}: {}", Utf8FromPath(mods.All()[held.index].ManifestPath()), held.reason));
    }
    return problems;
}

Result<void> CheckMods(const ModList& mods, const settings::Registry& registry,
                       std::vector<std::string> contentProblems) {
    std::vector<std::string> problems = ModProblems(mods, registry);
    std::ranges::move(contentProblems, std::back_inserter(problems));
    if (problems.empty()) {
        log::Info("checked {} mods, no problems", mods.All().size());
        return {};
    }
    std::string report = std::format("{} mod problems:", problems.size());
    for (const std::string& problem : problems) {
        log::Warn("{}", problem);
        report += std::format("\n{}", problem);
    }
    return Fail(ErrorCode::InvalidData, std::move(report));
}

}
