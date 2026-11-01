#include "sim/ai/route_choice.hpp"

#include <format>
#include <utility>

namespace rr::sim {
namespace {

constexpr std::string_view kPaces = "FMS";
constexpr int kMaxRoutesPerPace = 10;

}

std::vector<std::string> SlotRoutes(std::span<const assets::VfsEntry> folder, int slot) {
    std::vector<std::string> routes;
    for (const char pace : kPaces) {
        for (int number = 0; number < kMaxRoutesPerPace; ++number) {
            std::string name = std::format("R{}_{}_{}.RRB", slot, pace, number);
            if (!assets::HasFile(folder, name)) {
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

Result<GridRoutes> LoadGridRoutes(const assets::Vfs& vfs, std::string_view track,
                                  std::size_t slotCount, bool isMirrored) {
    Result<std::vector<assets::VfsEntry>> folder = vfs.List(std::format("GAMEDATA/{}", track));
    if (!folder) {
        return std::unexpected(std::move(folder.error()));
    }
    GridRoutes routes(slotCount);
    for (std::size_t slot = 0; slot < slotCount; ++slot) {
        for (const std::string& name : SlotRoutes(*folder, static_cast<int>(slot))) {
            Result<assets::RaceRoute> route = LoadRoute(vfs, track, name, isMirrored);
            if (!route) {
                return std::unexpected(std::move(route.error()));
            }
            routes[slot].push_back(std::move(*route));
        }
    }
    return routes;
}

}
