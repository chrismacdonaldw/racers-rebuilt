#include "assets/cutscene_timeline.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "assets/cutscene_tokens.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {

static Result<void> ReadValue(TokenReader& reader, CutsceneMaterialBinding& binding) {
    return ReadIndex(reader, binding.world)
        .and_then([&] { return ReadIndex(reader, binding.animation); })
        .and_then([&] { return ReadIndex(reader, binding.track); })
        .and_then([&] { return ReadIndex(reader, binding.slot); })
        .and_then([&] { return ReadIndex(reader, binding.materialTable); });
}

static Result<void> ReadValue(TokenReader& reader, LightBlink& blink) {
    return ReadMembers<&LightBlink::framesOn, &LightBlink::framesOff>(reader, blink);
}

namespace {

constexpr FieldId kFramesField = 0x27;
constexpr FieldId kWorldsField = 0x28;

template <typename Event>
constexpr FieldRule<Event> StartField() {
    return NestedField<&Event::span, &CutsceneSpan::start>(0x2B);
}

template <typename Event>
constexpr FieldRule<Event> DurationField() {
    return NestedField<&Event::span, &CutsceneSpan::duration>(0x2C);
}

constexpr std::array kCameraFields{
    StartField<CutsceneCamera>(),
    DurationField<CutsceneCamera>(),
    Field<&CutsceneCamera::camera>(0x2A),
    Field<&CutsceneCamera::part>(0x2D),
};

constexpr std::array kModelFields{
    StartField<CutsceneModel>(),
    DurationField<CutsceneModel>(),
    EntityNameField<&CutsceneModel::model, false>(0x2F),
    EntityNameField<&CutsceneModel::model, true>(0x30),
    UnsupportedField<CutsceneModel, 0x31>(),
    FieldRule<CutsceneModel>{.id = 0x32,
                             .read =
                                 [](TokenReader& reader, CutsceneModel& model) {
                                     auto& index = model.model.emplace<EntityByIndex>();
                                     return ReadIndex(reader, index.world).and_then([&] {
                                         return ReadIndex(reader, index.entity);
                                     });
                                 }},
    Field<&CutsceneModel::part>(0x2D),
    Field<&CutsceneModel::materials>(0x36),
    Field<&CutsceneModel::position>(0x33),
    Field<&CutsceneModel::direction, &CutsceneModel::up>(0x34),
};

constexpr std::array kTransformFields{
    StartField<CutsceneTransform>(),
    DurationField<CutsceneTransform>(),
    Field<&CutsceneTransform::position>(0x33),
    Field<&CutsceneTransform::direction, &CutsceneTransform::up>(0x34),
};

constexpr std::array kAmbientFields{
    StartField<CutsceneAmbientLight>(),
    DurationField<CutsceneAmbientLight>(),
    Field<&CutsceneAmbientLight::color>(0x38),
    Field<&CutsceneAmbientLight::blink>(0x3C),
};

constexpr std::array kDirectionalFields{
    StartField<CutsceneDirectionalLight>(),
    DurationField<CutsceneDirectionalLight>(),
    Field<&CutsceneDirectionalLight::color>(0x38),
    Field<&CutsceneDirectionalLight::direction>(0x39),
    Field<&CutsceneDirectionalLight::blink>(0x3C),
};

template <auto Events, FieldId Id, const auto& Rules>
constexpr FieldRule<CutsceneFrame> EventListField() {
    using Event =
        std::remove_reference_t<decltype(std::declval<CutsceneFrame&>().*Events)>::value_type;
    return {.id = Id, .read = [](TokenReader& reader, CutsceneFrame& frame) {
                return ReadNamedEntries<Event>(reader, Id, Rules, frame.*Events);
            }};
}

constexpr std::array kFrameFields{
    Field<&CutsceneFrame::framesPerSecond>(0x3B),
    Field<&CutsceneFrame::length>(0x2C),
    EventListField<&CutsceneFrame::cameras, 0x29, kCameraFields>(),
    EventListField<&CutsceneFrame::models, 0x2E, kModelFields>(),
    EventListField<&CutsceneFrame::transforms, 0x37, kTransformFields>(),
    EventListField<&CutsceneFrame::ambientLights, 0x35, kAmbientFields>(),
    EventListField<&CutsceneFrame::directionalLights, 0x3A, kDirectionalFields>(),
};

Result<void> ReadSection(TokenReader& reader, const Token& section,
                         CutsceneTimelineAsset& timeline) {
    switch (section.field) {
        case kWorldsField:
            return ReadNames(reader, timeline.worlds);
        case kFramesField:
            return ReadNamedEntries<CutsceneFrame>(reader, kFramesField, kFrameFields,
                                                   timeline.frames, FieldRepeats::Fail);
        default:
            return UnknownSection(section);
    }
}

std::int32_t LastCameraEnd(const CutsceneFrame& frame) {
    std::int32_t end = 0;
    for (const CutsceneCamera& camera : frame.cameras) {
        end = std::max(end, camera.span.End());
    }
    return end;
}

template <typename Event>
bool HasBadSpan(const std::vector<Event>& events) {
    return std::ranges::any_of(events, [](const Event& event) {
        const auto [start, duration] = event.span;
        return start < 0 || duration < 0 ||
               duration > std::numeric_limits<std::int32_t>::max() - start;
    });
}

std::optional<std::string> ModelProblem(const CutsceneModel& model, std::size_t worldCount) {
    if (const auto* byName = std::get_if<EntityByName>(&model.model);
        byName != nullptr && byName->name.empty()) {
        return "names no model";
    }
    if (const auto* byIndex = std::get_if<EntityByIndex>(&model.model);
        byIndex != nullptr && byIndex->world >= worldCount) {
        return "indexes a world past the list";
    }
    if (std::ranges::any_of(model.materials, [&](const CutsceneMaterialBinding& binding) {
            return binding.world >= worldCount;
        })) {
        return "binds a material animation from a world past the list";
    }
    return std::nullopt;
}

std::optional<std::string> FrameProblem(const CutsceneFrame& frame, std::size_t worldCount) {
    if (frame.framesPerSecond <= 0 || frame.length < 0) {
        return "its frame rate or length is out of range";
    }
    if (HasBadSpan(frame.cameras) || HasBadSpan(frame.models) || HasBadSpan(frame.transforms) ||
        HasBadSpan(frame.ambientLights) || HasBadSpan(frame.directionalLights)) {
        return "an event starts or lasts less than 0 frames, or ends past the last frame number";
    }
    for (const CutsceneModel& model : frame.models) {
        if (std::optional<std::string> problem = ModelProblem(model, worldCount)) {
            return std::format("model '{}' {}", model.name, *problem);
        }
    }
    return std::nullopt;
}

Result<void> Finish(CutsceneTimelineAsset& timeline) {
    if (timeline.frames.empty()) {
        return Fail(ErrorCode::InvalidData, "the cutscene has no frames");
    }
    for (CutsceneFrame& frame : timeline.frames) {
        if (std::optional<std::string> problem = FrameProblem(frame, timeline.worlds.size())) {
            return Fail(ErrorCode::InvalidData,
                        std::format("frame '{}' is invalid: {}", frame.name, *problem));
        }
        if (frame.length == 0) {
            frame.length = LastCameraEnd(frame);
        }
    }
    return {};
}

}

Result<CutsceneTimelineAsset> ParseCutsceneTimeline(std::span<const std::byte> bytes) {
    CutsceneTimelineAsset timeline;
    return ReadSections(bytes,
                        [&](TokenReader& reader, const Token& section) {
                            return ReadSection(reader, section, timeline);
                        })
        .and_then([&] { return Finish(timeline); })
        .transform([&] { return std::move(timeline); });
}

}
