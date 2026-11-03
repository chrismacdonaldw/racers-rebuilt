#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "assets/driver_look.hpp"
#include "assets/driver_parts.hpp"
#include "core/duration.hpp"
#include "settings/progress.hpp"

namespace rr::app {

enum class DriverSlot : std::uint8_t { Hat, Face, Torso, Legs };

inline constexpr std::size_t kDriverSlots = 4;

[[nodiscard]] std::uint8_t& PartOf(assets::DriverLook& look, DriverSlot slot);

/// The catalogue's parts in order, leaving out locked ones other than `current`.
[[nodiscard]] std::vector<std::uint8_t> UnlockedParts(const assets::DriverPartCatalog& catalog,
                                                      DriverSlot slot, std::uint8_t current,
                                                      const settings::Progress& progress);

struct CarouselShape {
    std::size_t slots = 0;
    std::size_t focused = 0;
};

enum class CarouselStep : std::uint8_t { Ignored, Moved, AtEnd };

/// Wraps unless it holds fewer parts than its slots but one.
class PartCarousel {
public:
    PartCarousel(std::vector<std::uint8_t> parts, std::uint8_t current, CarouselShape shape,
                 Milliseconds scroll);

    /// Ignored while the last step still scrolls.
    CarouselStep Step(int direction);

    /// Jumps without scrolling.
    void Select(std::size_t index);

    void Tick(Milliseconds elapsed);

    [[nodiscard]] std::size_t Count() const { return parts_.size(); }
    [[nodiscard]] std::uint8_t Part() const;
    [[nodiscard]] std::optional<std::uint8_t> PartInSlot(std::size_t slot) const;
    [[nodiscard]] const CarouselShape& Shape() const { return shape_; }
    [[nodiscard]] bool IsScrolling() const { return left_ > Milliseconds{0}; }

    /// The slot a part scrolls in from, or none when it's already in place.
    [[nodiscard]] std::optional<std::size_t> CameFrom(std::size_t slot) const;

    /// From 1 as a step starts to 0 when the parts are in place.
    [[nodiscard]] float Lag() const;

private:
    [[nodiscard]] bool Wraps() const;

    std::vector<std::uint8_t> parts_;
    CarouselShape shape_;
    Milliseconds scroll_;
    std::size_t selected_ = 0;
    Milliseconds left_{0};
    int direction_ = 0;
};

}
