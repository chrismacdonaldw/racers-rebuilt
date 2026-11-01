#include "assets/event_table.hpp"

#include <algorithm>
#include <format>
#include <string_view>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kRecordField = 0x27;
constexpr FieldId kEndTriggerField = 0x3C;
constexpr FieldId kLinkField = 0x39;

template <typename R>
constexpr FieldRule<R> NoEndField() {
    return NestedField<&R::trigger, &EventTrigger::ignoresEnd>(0x3A);
}

template <typename R>
constexpr FieldRule<R> EventPositionField() {
    return NestedField<&R::trigger, &EventTrigger::usesEventPosition>(0x3F);
}

Result<void> ReadSubEvent(TokenReader& reader, SubEvents& events) {
    Result<Token> selector = reader.Expect(TokenKind::Field);
    if (!selector) {
        return std::unexpected(std::move(selector.error()));
    }
    EventId* slot = nullptr;
    switch (selector->field) {
        case 0x36:
            slot = &events.start;
            break;
        case 0x34:
            slot = &events.active;
            break;
        case 0x37:
            slot = &events.end;
            break;
        default:
            return Fail(ErrorCode::InvalidData,
                        std::format("unknown sub-event slot 0x{:02X} at offset {}", selector->field,
                                    selector->offset));
    }
    return ReadValue(reader, *slot);
}

template <typename R>
constexpr FieldRule<R> SubEventField() {
    return {.id = kRecordField, .read = [](TokenReader& reader, R& resource) {
                return ReadSubEvent(reader, resource.trigger.subEvents);
            }};
}

template <typename R>
constexpr FieldRule<R> SharedDatabaseField() {
    return {.id = 0x41, .read = [](TokenReader&, R& resource) -> Result<void> {
                if (!resource.entity) {
                    resource.entity = EntityRef{.isShared = true};
                }
                return {};
            }};
}

template <typename R>
constexpr FieldRule<R> EntityField() {
    return {.id = 0x33, .read = [](TokenReader& reader, R& resource) {
                const bool isShared = resource.entity && resource.entity->isShared;
                return reader.ReadString().transform([&](std::string_view name) {
                    resource.entity = EntityRef{.name = std::string(name), .isShared = isShared};
                });
            }};
}

constexpr std::array kSoundFields{
    Field<&SoundResource::position>(0x3B),
    Field<&SoundResource::soundId>(0x2C),
    IgnoredField<SoundResource, std::int32_t>(0x2E),
    Field<&SoundResource::volume>(0x2F),
    Field<&SoundResource::frequencyScale>(0x30),
    Field<&SoundResource::minDistance>(0x31),
    Field<&SoundResource::maxDistance>(0x32),
    Field<&SoundResource::isLooping>(0x2D),
    NoEndField<SoundResource>(),
    EventPositionField<SoundResource>(),
    Field<&SoundResource::probability>(0x40),
    Field<&SoundResource::entity>(0x33),
    Field<&SoundResource::node>(0x54),
};

constexpr std::array kPartAnimationFields{
    EntityField<PartAnimationResource>(),
    SharedDatabaseField<PartAnimationResource>(),
    Field<&PartAnimationResource::activePart>(0x34),
    Field<&PartAnimationResource::idlePart>(0x35),
    Field<&PartAnimationResource::startPart>(0x36),
    Field<&PartAnimationResource::endPart>(0x37),
    Field<&PartAnimationResource::isLooping>(0x2D),
    NoEndField<PartAnimationResource>(),
    EventPositionField<PartAnimationResource>(),
    SubEventField<PartAnimationResource>(),
};

constexpr std::array kMaterialAnimationFields{
    Field<&MaterialAnimationResource::animation>(0x38),
    Field<&MaterialAnimationResource::activeTrack>(0x34),
    Field<&MaterialAnimationResource::idleTrack>(0x35),
    Field<&MaterialAnimationResource::isLooping>(0x2D),
    NoEndField<MaterialAnimationResource>(),
    EventPositionField<MaterialAnimationResource>(),
    FieldRule<MaterialAnimationResource>{
        .id = 0x33,
        .read =
            [](TokenReader& reader, MaterialAnimationResource& resource) {
                EntityMaterial& material = resource.material.emplace();
                return ReadValue(reader, material.entity).and_then([&] {
                    return ReadValue(reader, material.material);
                });
            }},
    SubEventField<MaterialAnimationResource>(),
};

constexpr std::array kParticleFields{
    Field<&ParticleResource::emitter>(0x3D),
    NoEndField<ParticleResource>(),
    EventPositionField<ParticleResource>(),
    Field<&ParticleResource::position>(0x3B),
    Field<&ParticleResource::direction, &ParticleResource::up>(0x3E),
    SharedDatabaseField<ParticleResource>(),
    EntityField<ParticleResource>(),
    Field<&ParticleResource::node>(0x54),
    SubEventField<ParticleResource>(),
};

constexpr std::array kSkyStateFields{
    Field<&SkyStateResource::sky>(0x43),
    Field<&SkyStateResource::fade>(0x44),
    Field<&SkyStateResource::hidesDome>(0x45),
    Field<&SkyStateResource::showsDome>(0x46),
    Field<&SkyStateResource::hidesSkyWorld>(0x47),
    Field<&SkyStateResource::showsSkyWorld>(0x48),
    SubEventField<SkyStateResource>(),
};

constexpr std::array kTimerFields{
    Field<&TimerResource::duration>(0x49),
    Field<&TimerResource::holdEvent>(0x4C),
    SubEventField<TimerResource>(),
};

constexpr std::array kNodeTransformFields{
    Field<&NodeTransformResource::triggerEntity>(0x4A),
    Field<&NodeTransformResource::entity>(0x33),
    Field<&NodeTransformResource::node>(0x54),
    SubEventField<NodeTransformResource>(),
};

constexpr std::array kColorTransformFields{
    Field<&ColorTransformResource::entity>(0x33),  Field<&ColorTransformResource::shifts>(0x4E),
    Field<&ColorTransformResource::offsets>(0x4F), Field<&ColorTransformResource::clears>(0x50),
    NoEndField<ColorTransformResource>(),          SubEventField<ColorTransformResource>(),
};

constexpr std::array kLapZoneFields{
    FlagField<&LapZoneResource::zone, LapZone::OpenTrack>(0x36),
    FlagField<&LapZoneResource::zone, LapZone::PostLine>(0x37),
    SubEventField<LapZoneResource>(),
};

constexpr std::array kModelDistanceFields{
    EntityField<ModelDistanceResource>(),
    NoEndField<ModelDistanceResource>(),
    SharedDatabaseField<ModelDistanceResource>(),
    Field<&ModelDistanceResource::hidesWhileActive>(0x46),
    SubEventField<ModelDistanceResource>(),
};

constexpr std::array kLookTargetFields{
    Field<&LookTargetResource::position>(0x3B),
    SubEventField<LookTargetResource>(),
};

constexpr std::array kExternalForceFields{
    Field<&ExternalForceResource::force>(0x3E),
    Field<&ExternalForceResource::channel>(0x4C),
    Field<&ExternalForceResource::armEvent>(0x5A),
    SubEventField<ExternalForceResource>(),
};

enum class EndTrigger : std::uint8_t { Accepted, Rejected };

Result<void> ReadEndTrigger(TokenReader& reader, EndTrigger accepts, bool& startsOnEnd) {
    Result<Token> next = reader.Peek();
    if (!next) {
        return std::unexpected(std::move(next.error()));
    }
    if (accepts == EndTrigger::Accepted && next->kind == TokenKind::Field &&
        next->field == kEndTriggerField) {
        startsOnEnd = true;
        return reader.Next().transform([](const Token&) {});
    }
    return {};
}

template <typename R>
Result<void> ReadRecords(TokenReader& reader, std::span<const FieldRule<R>> rules,
                         EndTrigger accepts, std::vector<R>& out) {
    return ReadList(reader, [&] {
        R& resource = out.emplace_back();
        return ExpectField(reader, kRecordField)
            .and_then([&] { return ReadValue(reader, resource.trigger.event); })
            .and_then([&] { return ReadEndTrigger(reader, accepts, resource.trigger.startsOnEnd); })
            .and_then([&] { return ReadBlock<R>(reader, rules, resource); });
    });
}

Result<void> ReadLinks(TokenReader& reader, std::vector<EventLink>& out) {
    return ReadList(reader, [&] {
        EventLink& link = out.emplace_back();
        return ExpectField(reader, kRecordField)
            .and_then([&] { return ReadValue(reader, link.event); })
            .and_then([&] { return reader.Expect(TokenKind::OpenBrace); })
            .and_then([&](const Token&) { return ExpectField(reader, kLinkField); })
            .and_then([&] { return ReadValue(reader, link.forced); })
            .and_then([&] { return reader.Expect(TokenKind::CloseBrace); })
            .transform([](const Token&) {});
    });
}

Result<void> ReadSection(TokenReader& reader, const Token& section, EventTableAsset& table) {
    using enum EndTrigger;
    switch (section.field) {
        case 0x2A:
            return ReadRecords<SoundResource>(reader, kSoundFields, Accepted, table.sounds);
        case 0x28:
            return ReadRecords<PartAnimationResource>(reader, kPartAnimationFields, Accepted,
                                                      table.partAnimations);
        case 0x29:
            return ReadRecords<MaterialAnimationResource>(reader, kMaterialAnimationFields,
                                                          Accepted, table.materialAnimations);
        case 0x3D:
            return ReadRecords<ParticleResource>(reader, kParticleFields, Accepted,
                                                 table.particles);
        case 0x39:
            return ReadLinks(reader, table.links);
        case 0x42:
            return ReadRecords<SkyStateResource>(reader, kSkyStateFields, Accepted,
                                                 table.skyStates);
        case 0x4B:
            return ReadRecords<TimerResource>(reader, kTimerFields, Accepted, table.timers);
        case 0x52:
            return ReadRecords<NodeTransformResource>(reader, kNodeTransformFields, Accepted,
                                                      table.nodeTransforms);
        case 0x4D:
            return ReadRecords<ColorTransformResource>(reader, kColorTransformFields, Accepted,
                                                       table.colorTransforms);
        case 0x51:
            return ReadRecords<LapZoneResource>(reader, kLapZoneFields, Rejected, table.lapZones);
        case 0x53:
            return ReadRecords<ModelDistanceResource>(reader, kModelDistanceFields, Accepted,
                                                      table.modelDistances);
        case 0x55:
            return ReadRecords<LookTargetResource>(reader, kLookTargetFields, Rejected,
                                                   table.lookTargets);
        case 0x59:
            return ReadRecords<ExternalForceResource>(reader, kExternalForceFields, Accepted,
                                                      table.externalForces);
        default:
            return Fail(ErrorCode::InvalidData, std::format("unknown section 0x{:02X} at offset {}",
                                                            section.field, section.offset));
    }
}

template <typename R>
void DropUnnamedEntities(std::vector<R>& resources) {
    for (R& resource : resources) {
        if (resource.entity && resource.entity->name.empty()) {
            resource.entity.reset();
        }
    }
}

void Finish(EventTableAsset& table) {
    DropUnnamedEntities(table.partAnimations);
    DropUnnamedEntities(table.particles);
    DropUnnamedEntities(table.modelDistances);
    for (ColorTransformResource& color : table.colorTransforms) {
        color.trigger.ignoresEnd = color.trigger.ignoresEnd || color.trigger.startsOnEnd;
    }
}

}

Result<EventTableAsset> ParseEventTable(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    EventTableAsset table;
    std::vector<FieldId> seen;
    while (true) {
        Result<Token> section = reader.Next();
        if (!section) {
            return std::unexpected(std::move(section.error()));
        }
        if (section->kind == TokenKind::End) {
            break;
        }
        if (section->kind != TokenKind::Field || std::ranges::contains(seen, section->field)) {
            return Fail(ErrorCode::InvalidData,
                        std::format("expected a new section at offset {}", section->offset));
        }
        seen.push_back(section->field);
        if (Result<void> read = ReadSection(reader, *section, table); !read) {
            return std::unexpected(std::move(read.error()));
        }
    }
    Finish(table);
    return table;
}

}
