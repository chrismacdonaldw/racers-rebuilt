#include "assets/surface_table.hpp"

#include <array>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kSurfaceField = 0x27;

constexpr std::array kSurfaceFields{
    Field<&SurfaceEntry::enterEvent>(0x28),
    Field<&SurfaceEntry::leaveEvent>(0x29),
    Field<&SurfaceEntry::touchEvent>(0x2A),
    Field<&SurfaceEntry::projectileEvent>(0x2B),
    Field<&SurfaceEntry::vector2C>(0x2C),
    Field<&SurfaceEntry::force>(0x2D),
    Field<&SurfaceEntry::soundId>(0x2E),
    Field<&SurfaceEntry::value2F>(0x2F),
    Field<&SurfaceEntry::value30>(0x30),
    Field<&SurfaceEntry::particle>(0x31),
    Field<&SurfaceEntry::supportThreshold>(0x32),
    Field<&SurfaceEntry::friction>(0x33),
    Field<&SurfaceEntry::lateralGrip>(0x34),
    Field<&SurfaceEntry::value35>(0x35),
    Field<&SurfaceEntry::rollingResistance>(0x36),
    Field<&SurfaceEntry::isNonSolid>(0x37),
    Field<&SurfaceEntry::isProjectilePassable>(0x38),
    Field<&SurfaceEntry::isFinish>(0x39),
};

}

Result<std::vector<SurfaceEntry>> ParseSurfaceTable(std::span<const std::byte> bytes) {
    return ParseNamedLibrary<SurfaceEntry>(bytes, kSurfaceField, kSurfaceFields);
}

}
