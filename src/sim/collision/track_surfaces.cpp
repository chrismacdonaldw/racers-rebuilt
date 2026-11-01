#include "sim/collision/track_surfaces.hpp"

#include <algorithm>

namespace rr::sim {

TrackSurfaces::TrackSurfaces(std::span<const assets::SurfaceEntry> entries)
    : entries_(entries.begin(), entries.end()) {
    flags_.reserve(entries_.size());
    for (const assets::SurfaceEntry& entry : entries_) {
        flags_.push_back(
            {.isNonSolid = entry.isNonSolid, .isShotPassable = entry.isProjectilePassable});
    }
}

std::optional<std::size_t> TrackSurfaces::Find(std::string_view name) const {
    const auto found = std::ranges::find(entries_, name, &assets::SurfaceEntry::name);
    if (found == entries_.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - entries_.begin());
}

bool TrackSurfaces::IsSolid(std::size_t surface) const {
    return !flags_.at(surface).isNonSolid;
}

bool TrackSurfaces::LetsShotsThrough(std::size_t surface) const {
    return flags_.at(surface).isShotPassable;
}

void TrackSurfaces::SetPassable(std::size_t surface, bool isPassable) {
    flags_.at(surface) = {.isNonSolid = isPassable, .isShotPassable = isPassable};
}

}
