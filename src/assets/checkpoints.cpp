#include "assets/checkpoints.hpp"

#include <array>
#include <format>
#include <optional>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kListField = 0x27;
constexpr std::size_t kMaxCheckpoints = 254;

struct StoredCheckpoint {
    std::optional<std::array<float, 4>> plane;
    std::optional<Vector3> position;
    std::optional<std::array<std::uint8_t, 4>> links;
};

constexpr std::array kCheckpointFields{
    Field<&StoredCheckpoint::plane>(0x28),
    Field<&StoredCheckpoint::links>(0x29),
    Field<&StoredCheckpoint::position>(0x2A),
};

Result<Checkpoint> Decode(const StoredCheckpoint& stored) {
    if (!stored.plane || !stored.position || !stored.links) {
        return Fail(ErrorCode::InvalidData, "checkpoint is missing its plane, position or links");
    }
    const auto& [a, b, c, d] = *stored.plane;
    return Checkpoint{.normal = {.x = a, .y = b, .z = c},
                      .distance = d,
                      .position = *stored.position,
                      .links = *stored.links};
}

Result<void> CheckLinks(const std::vector<Checkpoint>& checkpoints) {
    if (checkpoints.empty() || checkpoints.size() > kMaxCheckpoints) {
        return Fail(ErrorCode::InvalidData, std::format("{} checkpoints, wanted 1 to {}",
                                                        checkpoints.size(), kMaxCheckpoints));
    }
    for (std::size_t i = 0; i < checkpoints.size(); ++i) {
        const auto& links = checkpoints[i].links;
        for (std::size_t slot = 0; slot < links.size(); ++slot) {
            const bool isOptional = slot > 0 && links.at(slot) == kNoLink;
            if (!isOptional && links.at(slot) >= checkpoints.size()) {
                return Fail(ErrorCode::InvalidData,
                            std::format("checkpoint {} links to {} of {}", i, links.at(slot),
                                        checkpoints.size()));
            }
        }
    }
    return {};
}

}

Result<CheckpointsAsset> ParseCpb(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    CheckpointsAsset asset;
    const auto readCheckpoint = [&] {
        StoredCheckpoint stored;
        return ExpectField(reader, kListField)
            .and_then(
                [&] { return ReadBlock<StoredCheckpoint>(reader, kCheckpointFields, stored); })
            .and_then([&] { return Decode(stored); })
            .transform(
                [&](const Checkpoint& checkpoint) { asset.checkpoints.push_back(checkpoint); });
    };
    return ExpectField(reader, kListField)
        .and_then([&] { return ReadList(reader, readCheckpoint); })
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .and_then([&](const Token&) { return CheckLinks(asset.checkpoints); })
        .transform([&] { return std::move(asset); });
}

}
