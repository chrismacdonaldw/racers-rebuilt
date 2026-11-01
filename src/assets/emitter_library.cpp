#include "assets/emitter_library.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <ranges>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kEmitterField = 0x27;
constexpr std::size_t kMaxNameLength = 8;
constexpr std::int32_t kRunsUntilStopped = -1;

struct StoredEmitter {
    std::string name;
    float emitRate = 0.0F;
    float emitChance = 0.0F;
    Vector3 acceleration;
    std::vector<Vector3> points;
    float startSizeUp = 1.0F;
    float startSizeForward = 1.0F;
    std::optional<std::int32_t> materialTrack;
    std::int32_t particleLifeMs = 0;
    std::int32_t durationMs = kRunsUntilStopped;
    float sizeGrowthUp = 0.0F;
    float sizeGrowthForward = 0.0F;
    float radius = 0.0F;
    std::optional<std::string> material;
    std::uint8_t priority = 0;
};

constexpr std::array kEmitterFields{
    Field<&StoredEmitter::emitRate>(0x28),          Field<&StoredEmitter::emitChance>(0x29),
    Field<&StoredEmitter::acceleration>(0x2A),      Field<&StoredEmitter::points>(0x2B),
    Field<&StoredEmitter::startSizeUp>(0x2C),       Field<&StoredEmitter::startSizeForward>(0x2D),
    Field<&StoredEmitter::materialTrack>(0x2E),     Field<&StoredEmitter::particleLifeMs>(0x2F),
    Field<&StoredEmitter::durationMs>(0x30),        Field<&StoredEmitter::sizeGrowthUp>(0x31),
    Field<&StoredEmitter::sizeGrowthForward>(0x32), Field<&StoredEmitter::radius>(0x33),
    Field<&StoredEmitter::material>(0x34),          Field<&StoredEmitter::priority>(0x35),
};

std::optional<std::string> Problem(const StoredEmitter& stored) {
    if (stored.name.size() > kMaxNameLength ||
        (stored.material && stored.material->size() > kMaxNameLength)) {
        return "a name is longer than 8 characters";
    }
    if (stored.points.empty()) {
        return "it has no spawn velocities";
    }
    if (stored.materialTrack && *stored.materialTrack < 0) {
        return "its material track is negative";
    }
    if (stored.particleLifeMs < 0 || stored.durationMs < kRunsUntilStopped) {
        return "its particle life or duration is negative";
    }
    return std::nullopt;
}

Result<EmitterAsset> Convert(StoredEmitter stored) {
    if (const std::optional<std::string> problem = Problem(stored)) {
        return Fail(ErrorCode::InvalidData,
                    std::format("emitter '{}' is invalid: {}", stored.name, *problem));
    }
    return EmitterAsset{
        .name = std::move(stored.name),
        .emitRate = stored.emitRate,
        .emitChance = stored.emitChance,
        .acceleration = stored.acceleration,
        .points = std::move(stored.points),
        .startSizeUp = stored.startSizeUp,
        .startSizeForward = stored.startSizeForward,
        .sizeGrowthUp = stored.sizeGrowthUp,
        .sizeGrowthForward = stored.sizeGrowthForward,
        .materialTrack = stored.materialTrack.transform(
            [](std::int32_t track) { return static_cast<std::size_t>(track); }),
        .material = std::move(stored.material),
        .particleLife = Milliseconds{stored.particleLifeMs},
        .duration = stored.durationMs == kRunsUntilStopped
                        ? std::nullopt
                        : std::optional<Milliseconds>{Milliseconds{stored.durationMs}},
        .radius = stored.radius,
        .priority = stored.priority,
    };
}

bool HasDuplicateName(std::vector<std::string> names) {
    std::ranges::sort(names);
    return std::ranges::adjacent_find(names) != names.end();
}

}

Result<std::vector<EmitterAsset>> ParseEmitterLibrary(std::span<const std::byte> bytes) {
    Result<std::vector<StoredEmitter>> stored =
        ParseNamedLibrary<StoredEmitter>(bytes, kEmitterField, kEmitterFields);
    if (!stored) {
        return std::unexpected(std::move(stored.error()));
    }
    if (HasDuplicateName(*stored | std::views::transform(&StoredEmitter::name) |
                         std::ranges::to<std::vector>())) {
        return Fail(ErrorCode::InvalidData, "two emitters share a name");
    }
    std::vector<EmitterAsset> emitters;
    emitters.reserve(stored->size());
    for (StoredEmitter& emitter : *stored) {
        Result<EmitterAsset> converted = Convert(std::move(emitter));
        if (!converted) {
            return std::unexpected(std::move(converted.error()));
        }
        emitters.push_back(std::move(*converted));
    }
    return emitters;
}

}
