#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "app/menu/build_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "assets/car_data.hpp"
#include "assets/driver_parts.hpp"
#include "assets/vfs.hpp"
#include "render/render_view.hpp"
#include "settings/racers.hpp"

namespace rr::app {

inline constexpr std::int32_t kLockedSound = kBuildRefusedSound;

enum class RacerLock : std::uint8_t { None, ExtraPieces, MissingPieces, MissingParts };

/// A car with extra pieces can't race or be saved while their setting is off.
[[nodiscard]] bool IsLockedCar(const assets::CarData& car, bool allowsExtraPieces);

/// A car with extra pieces while they're off, a car or driver with a piece or part from a mod
/// that's off, keeps a racer from racing.
class RacerLocks {
public:
    explicit RacerLocks(std::optional<assets::DriverPartCatalog> parts,
                        std::optional<std::vector<std::uint16_t>> pieceTypes = std::nullopt);

    /// Locks no driver when the parts don't load, and no car when the pieces don't.
    [[nodiscard]] static RacerLocks Load(const assets::Vfs& vfs);

    void AllowExtraPieces(bool allows) { allowsExtraPieces_ = allows; }
    [[nodiscard]] RacerLock Of(const settings::Racer& racer) const;

private:
    std::optional<assets::DriverPartCatalog> parts_;
    std::optional<std::vector<std::uint16_t>> pieceTypes_;
    bool allowsExtraPieces_ = false;
};

/// Says why the racer is locked, along the bottom of `area`.
void DrawLockNotice(const MenuText& text, RacerLock lock, const render::Rect& area,
                    std::vector<render::SpriteDraw>& out);

}
