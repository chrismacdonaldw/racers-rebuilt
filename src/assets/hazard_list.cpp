#include "assets/hazard_list.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kListField = 0x27;
constexpr FieldId kCannonField = 0x33;
constexpr FieldId kLaunchField = 0x37;
constexpr FieldId kTargetField = 0x38;
constexpr FieldId kTriggerCentreField = 0x39;
constexpr FieldId kTriggerRadiusField = 0x3A;
constexpr FieldId kTriggerField = 0x3B;
constexpr FieldId kSurfaceField = 0x41;
constexpr FieldId kEntityField = 0x42;
constexpr FieldId kIntervalField = 0x44;
constexpr FieldId kMaterialField = 0x45;
constexpr FieldId kPeriodField = 0x46;
constexpr FieldId kSweepField = 0x47;

constexpr std::array kCannonFields{
    Field<&CannonShot::launch>(kLaunchField),
    Field<&CannonShot::target>(kTargetField),
    Field<&CannonShot::triggerCentre>(kTriggerCentreField),
    Field<&CannonShot::triggerRadius>(kTriggerRadiusField),
    Field<&CannonShot::event>(kTriggerField),
};

template <typename Payload, const auto& Rules>
Result<void> ReadFieldBlock(TokenReader& reader, Payload& payload) {
    return ReadBlock<Payload>(reader, Rules, payload, FieldRepeats::Fail);
}

Result<void> ReadPropEntities(TokenReader& reader, BreakableProp& prop) {
    while (true) {
        if (prop.entities.size() == kMaxPropEntities) {
            return Fail(
                ErrorCode::InvalidData,
                std::format("a breakable prop names more than {} entities", kMaxPropEntities));
        }
        Result<Token> next =
            ReadValue(reader, prop.entities.emplace_back()).and_then([&] { return reader.Peek(); });
        if (!next) {
            return std::unexpected(std::move(next.error()));
        }
        if (next->kind != TokenKind::Field || next->field != kEntityField) {
            return {};
        }
        if (Result<Token> field = reader.Expect(TokenKind::Field); !field) {
            return std::unexpected(std::move(field.error()));
        }
    }
}

constexpr std::array kPropFields{
    Field<&BreakableProp::surface>(kSurfaceField),
    FieldRule<BreakableProp>{.id = kEntityField, .read = ReadPropEntities},
    Field<&BreakableProp::trigger>(kTriggerField),
};

template <auto Points>
Result<void> ReadAimPoints(TokenReader& reader, ShipCannons& cannons) {
    std::vector<AimPoint>& points = cannons.*Points;
    points.clear();
    return ReadList(reader, [&] {
        return ReadMembers<&AimPoint::position, &AimPoint::event>(reader, points.emplace_back());
    });
}

Result<void> ReadShipShot(TokenReader& reader, ShipCannons& cannons) {
    return ReadFieldBlock<CannonShot, kCannonFields>(reader, cannons.shot);
}

constexpr std::array kShipFields{
    FieldRule<ShipCannons>{.id = kLaunchField, .read = ReadAimPoints<&ShipCannons::launches>},
    FieldRule<ShipCannons>{.id = kTargetField, .read = ReadAimPoints<&ShipCannons::targets>},
    FieldRule<ShipCannons>{.id = kCannonField, .read = ReadShipShot},
};

constexpr std::array kLightningFields{
    Field<&LightningCannon::trigger>(kTriggerField),
    Field<&LightningCannon::entity>(kEntityField),
    Field<&LightningCannon::source>(kLaunchField),
    Field<&LightningCannon::period>(kPeriodField),
    Field<&LightningCannon::sweepRadians, &LightningCannon::verticalRange,
          &LightningCannon::verticalBase>(kSweepField),
};

constexpr std::array kDragonFields{
    Field<&DragonCannonballs::material>(kMaterialField),
    Field<&DragonCannonballs::entity>(kEntityField),
    Field<&DragonCannonballs::trigger>(kTriggerField),
    Field<&DragonCannonballs::interval>(kIntervalField),
};

constexpr std::array kUfoFields{
    Field<&UfoGrabber::trigger>(kTriggerField),
    Field<&UfoGrabber::entity, &UfoGrabber::pullStrength, &UfoGrabber::grabFrameLow,
          &UfoGrabber::grabFrameHigh>(kEntityField),
};

Result<void> ReadWaterScroll(TokenReader& reader, WaterScroll& scroll) {
    return ReadMembers<&WaterScroll::entity, &WaterScroll::uAmplitude, &WaterScroll::vAmplitude>(
        reader, scroll);
}

Result<void> ReadSwingingBox(TokenReader& reader, SwingingBox& box) {
    return ReadMemberBlock<&SwingingBox::entity, &SwingingBox::trigger, &SwingingBox::startFrame,
                           &SwingingBox::size>(reader, box);
}

struct HazardSchema {
    HazardKind kind = HazardKind::FallingPillar;
    Result<void> (*read)(TokenReader& reader, HazardPayload& payload) = nullptr;
};

constexpr HazardSchema Bare(HazardKind kind) {
    return {.kind = kind, .read = [](TokenReader&, HazardPayload&) -> Result<void> { return {}; }};
}

template <typename Payload, Result<void> (*Read)(TokenReader&, Payload&)>
constexpr HazardSchema WithPayload(HazardKind kind) {
    return {.kind = kind, .read = [](TokenReader& reader, HazardPayload& payload) {
                return Read(reader, payload.emplace<Payload>());
            }};
}

constexpr std::array kSchemas{
    Bare(HazardKind::FallingPillar),
    Bare(HazardKind::Sphinx),
    Bare(HazardKind::HammerSwish),
    Bare(HazardKind::Ghost),
    Bare(HazardKind::LavaBlob),
    Bare(HazardKind::CodePuzzle),
    Bare(HazardKind::RocketPad),
    Bare(HazardKind::Snowfall),
    Bare(HazardKind::SmokeVent),
    Bare(HazardKind::Crane),
    WithPayload<CannonShot, ReadFieldBlock<CannonShot, kCannonFields>>(HazardKind::Cannon),
    WithPayload<BreakableProp, ReadFieldBlock<BreakableProp, kPropFields>>(
        HazardKind::BreakableProp),
    WithPayload<WaterScroll, ReadWaterScroll>(HazardKind::WaterScroll),
    WithPayload<ShipCannons, ReadFieldBlock<ShipCannons, kShipFields>>(HazardKind::ShipCannons),
    WithPayload<SwingingBox, ReadSwingingBox>(HazardKind::SwingingBox),
    Bare(HazardKind::CurseDrop),
    WithPayload<LightningCannon, ReadFieldBlock<LightningCannon, kLightningFields>>(
        HazardKind::LightningCannon),
    WithPayload<DragonCannonballs, ReadFieldBlock<DragonCannonballs, kDragonFields>>(
        HazardKind::DragonCannonballs),
    WithPayload<UfoGrabber, ReadFieldBlock<UfoGrabber, kUfoFields>>(HazardKind::UfoGrabber),
    Bare(HazardKind::WarpPad),
};

Result<void> ReadHazard(TokenReader& reader, std::vector<Hazard>& hazards) {
    Result<Token> field = reader.Expect(TokenKind::Field);
    if (!field) {
        return std::unexpected(std::move(field.error()));
    }
    const auto schema = std::ranges::find_if(kSchemas, [&](const HazardSchema& candidate) {
        return static_cast<FieldId>(candidate.kind) == field->field;
    });
    if (schema == kSchemas.end()) {
        return Fail(
            ErrorCode::InvalidData,
            std::format("unknown hazard class 0x{:02X} at offset {}", field->field, field->offset));
    }
    return schema->read(reader, hazards.emplace_back(Hazard{.kind = schema->kind}).payload);
}

}

Result<std::vector<Hazard>> ParseHazards(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    std::vector<Hazard> hazards;
    return ExpectField(reader, kListField)
        .and_then(
            [&] { return ReadEntryList(reader, [&] { return ReadHazard(reader, hazards); }); })
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .transform([&](const Token&) { return std::move(hazards); });
}

}
