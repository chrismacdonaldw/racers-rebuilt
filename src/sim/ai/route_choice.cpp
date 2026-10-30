#include "sim/ai/route_choice.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include "assets/game_path.hpp"

namespace rr::sim {
namespace {

constexpr std::string_view kPaces = "FMS";
constexpr int kMaxRoutesPerPace = 10;

bool HasFile(std::span<const assets::VfsEntry> folder, std::string_view name) {
    return std::ranges::any_of(folder, [&](const assets::VfsEntry& entry) {
        return entry.kind == assets::EntryKind::File && assets::ToUpperAscii(entry.name) == name;
    });
}

}

std::vector<std::string> SlotRoutes(std::span<const assets::VfsEntry> folder, int slot) {
    std::vector<std::string> routes;
    for (const char pace : kPaces) {
        for (int number = 0; number < kMaxRoutesPerPace; ++number) {
            std::string name = std::format("R{}_{}_{}.RRB", slot, pace, number);
            if (!HasFile(folder, name)) {
                break;
            }
            routes.push_back(std::move(name));
        }
    }
    return routes;
}

Result<assets::RaceRoute> LoadRoute(const assets::Vfs& vfs, std::string_view track,
                                    std::string_view name, bool isMirrored) {
    return vfs.Read(std::format("GAMEDATA/{}/{}", track, name))
        .and_then([](const assets::FileData& file) { return assets::ParseRaceRoute(file.Bytes()); })
        .transform([&](assets::RaceRoute route) {
            return isMirrored ? assets::Mirrored(std::move(route)) : std::move(route);
        });
}

}
