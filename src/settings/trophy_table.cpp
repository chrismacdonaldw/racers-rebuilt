#include "settings/trophy_table.hpp"

#include <algorithm>
#include <array>

#include "core/log.hpp"

namespace rr::settings {
namespace {

struct TrophyName {
    Trophy trophy;
    std::string_view name;
};

constexpr std::array kTrophyNames{TrophyName{.trophy = Trophy::Gold, .name = "gold"},
                                  TrophyName{.trophy = Trophy::Silver, .name = "silver"},
                                  TrophyName{.trophy = Trophy::Bronze, .name = "bronze"}};

}

Trophies ReadTrophyTable(const toml::table& table, std::string_view fileName) {
    Trophies trophies;
    for (const auto& [circuit, node] : table) {
        const std::string_view name = node.value_or(std::string_view{});
        const auto found = std::ranges::find(kTrophyNames, name, &TrophyName::name);
        if (found == kTrophyNames.end()) {
            log::Warn("{}: {} doesn't parse, skipping it", fileName, circuit.str());
            continue;
        }
        trophies.emplace(circuit.str(), found->trophy);
    }
    return trophies;
}

toml::table TrophyTable(const Trophies& trophies) {
    toml::table table;
    for (const auto& [circuit, trophy] : trophies) {
        table.insert(circuit, std::ranges::find(kTrophyNames, trophy, &TrophyName::trophy)->name);
    }
    return table;
}

}
