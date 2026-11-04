#include "app/mod_content.hpp"

#include <algorithm>
#include <format>
#include <iterator>

#include "app/racer/racer_lookup.hpp"
#include "app/track_info.hpp"
#include "assets/circuit_table.hpp"
#include "assets/driver_table.hpp"
#include "assets/racer_catalog.hpp"

namespace rr::app {
namespace {

void AddDriverProblems(const assets::Vfs& vfs, std::vector<std::string>& problems) {
    std::vector<std::string> skipped;
    const Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(vfs, skipped);
    std::ranges::move(skipped, std::back_inserter(problems));
    if (!catalog) {
        problems.push_back(catalog.error().message);
        return;
    }
    for (const assets::DriverEntry& driver : catalog->Drivers()) {
        if (!FindDriverRacer(vfs, driver)) {
            problems.push_back(std::format("{}: driver {}: there's no racer {}",
                                           assets::kDriverTablePath, driver.name, driver.racer));
        }
    }
}

}

std::vector<std::string> ModContentProblems(const assets::Vfs& vfs) {
    std::vector<std::string> problems = TrackInfoProblems(vfs);
    std::ranges::move(assets::CircuitTableProblems(vfs), std::back_inserter(problems));
    AddDriverProblems(vfs, problems);
    return problems;
}

}
