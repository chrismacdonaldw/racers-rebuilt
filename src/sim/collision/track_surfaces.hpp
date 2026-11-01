#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "assets/surface_table.hpp"

namespace rr::sim {

class TrackSurfaces {
public:
    TrackSurfaces() = default;
    explicit TrackSurfaces(std::span<const assets::SurfaceEntry> entries);

    [[nodiscard]] std::size_t Count() const { return entries_.size(); }
    [[nodiscard]] const assets::SurfaceEntry& Entry(std::size_t surface) const {
        return entries_.at(surface);
    }
    [[nodiscard]] std::optional<std::size_t> Find(std::string_view name) const;

    [[nodiscard]] bool IsSolid(std::size_t surface) const;
    [[nodiscard]] bool LetsShotsThrough(std::size_t surface) const;

    /// Racers and shots both pass a passable surface.
    void SetPassable(std::size_t surface, bool isPassable);

private:
    struct Flags {
        bool isNonSolid = false;
        bool isShotPassable = false;
    };

    std::vector<assets::SurfaceEntry> entries_;
    std::vector<Flags> flags_;
};

}
