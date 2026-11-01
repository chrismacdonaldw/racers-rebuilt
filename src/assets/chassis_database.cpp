#include "assets/chassis_database.hpp"

#include <array>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kChassisField = 0x27;

constexpr std::array kModelFields{
    NestedField<&JointedModelFiles::model, &ModelFiles::materials>(0x34),
    NestedField<&JointedModelFiles::model, &ModelFiles::textures>(0x35),
    NestedField<&JointedModelFiles::model, &ModelFiles::geometry>(0x36),
    Field<&JointedModelFiles::skeleton>(0x37),
    Field<&JointedModelFiles::animations>(0x38),
};

Result<void> ReadWheelModel(TokenReader& reader, ChassisEntry& chassis) {
    return ReadBlock<JointedModelFiles>(reader, kModelFields, chassis.wheels);
}

Result<void> ReadBodyAttachment(TokenReader& reader, ChassisEntry& chassis) {
    return ReadBlock<JointedModelFiles>(reader, kModelFields, chassis.bodyAttachment.emplace());
}

Result<void> ReadSkids(TokenReader& reader, ChassisEntry& chassis) {
    return ReadMemberBlock<&SkidMarks::frontWidth, &SkidMarks::rearWidth, &SkidMarks::wheelOffsets>(
        reader, chassis.skids);
}

Result<void> ReadWheelPositions(TokenReader& reader, ChassisEntry& chassis) {
    return ReadMemberBlock<&ChassisEntry::wheelPositions>(reader, chassis);
}

constexpr std::array kChassisFields{
    FieldRule<ChassisEntry>{.id = 0x28, .read = ReadWheelModel},
    FieldRule<ChassisEntry>{.id = 0x29, .read = ReadBodyAttachment},
    Field<&ChassisEntry::builtCentreOfMass>(0x2A),
    Field<&ChassisEntry::seat>(0x2B),
    Field<&ChassisEntry::baseMass>(0x2C),
    IgnoredField<ChassisEntry, float, float>(0x2D),
    Field<&ChassisEntry::shadowWidth, &ChassisEntry::shadowLength>(0x2E),
    Field<&ChassisEntry::enginePitchScale>(0x2F),
    FieldRule<ChassisEntry>{.id = 0x30, .read = ReadSkids},
    FieldRule<ChassisEntry>{.id = 0x31, .read = ReadWheelPositions},
    IgnoredField<ChassisEntry, std::int32_t>(0x32),
    IgnoredField<ChassisEntry, std::int32_t>(0x33),
    Field<&ChassisEntry::shadowTexture>(0x39),
    Field<&ChassisEntry::handling>(0x3A),
    Field<&ChassisEntry::topSpeed>(0x3B),
    Field<&ChassisEntry::acceleration>(0x3C),
};

}

Result<std::vector<ChassisEntry>> ParseChassis(std::span<const std::byte> bytes) {
    return ParseNamedLibrary<ChassisEntry>(bytes, kChassisField, kChassisFields);
}

}
