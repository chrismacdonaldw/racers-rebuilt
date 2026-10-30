#include "assets/driver_database.hpp"

#include <array>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kDriverField = 0x27;

constexpr std::array kDriverFields{
    NestedField<&DriverEntry::model, &ModelFiles::materials>(0x28),
    NestedField<&DriverEntry::model, &ModelFiles::textures>(0x29),
    NestedField<&DriverEntry::model, &ModelFiles::geometry>(0x2A),
    Field<&DriverEntry::champion>(0x2B),
    Field<&DriverEntry::redStat>(0x2C),
    Field<&DriverEntry::yellowStat>(0x2D),
    Field<&DriverEntry::greenStat>(0x2E),
    Field<&DriverEntry::blueStat>(0x2F),
    IgnoredField<DriverEntry, std::int32_t>(0x30),
    IgnoredField<DriverEntry, std::int32_t>(0x31),
    Field<&DriverEntry::nameIndex>(0x33),
    Field<&DriverEntry::voiceBank>(0x34),
    Field<&DriverEntry::hatIndex>(0x35),
    Field<&DriverEntry::faceIndex>(0x36),
    Field<&DriverEntry::torsoIndex>(0x37),
    Field<&DriverEntry::legsIndex>(0x38),
    Field<&DriverEntry::expressionIndex>(0x39),
    Field<&DriverEntry::aiChargeColor, &DriverEntry::aiWhiteBrickTarget>(0x3A),
};

}

Result<std::vector<DriverEntry>> ParseDrivers(std::span<const std::byte> bytes) {
    return ParseNamedLibrary<DriverEntry>(bytes, kDriverField, kDriverFields);
}

}
