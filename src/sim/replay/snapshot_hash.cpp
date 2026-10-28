#include "sim/replay/snapshot_hash.hpp"

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <initializer_list>

#include "sim/replay/replay_bytes.hpp"

namespace rr::sim {
namespace {

constexpr std::string_view kMagic = "RRRH";
constexpr std::uint16_t kVersion = 1;
constexpr std::size_t kHashBytes = 8;

template <std::unsigned_integral T>
std::uint64_t Mix(std::uint64_t hash, T value) {
    std::array<std::byte, sizeof(T)> bytes{};
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        bytes.at(i) = static_cast<std::byte>((value >> (8U * i)) & 0xFFU);
    }
    return HashBytes(bytes, hash);
}

std::uint64_t MixFloats(std::uint64_t hash, std::initializer_list<float> values) {
    for (const float value : values) {
        hash = Mix(hash, std::bit_cast<std::uint32_t>(value));
    }
    return hash;
}

std::uint64_t MixTransform(std::uint64_t hash, const Transform& transform) {
    return MixFloats(hash, {transform.position.x, transform.position.y, transform.position.z,
                            transform.rotation.w, transform.rotation.x, transform.rotation.y,
                            transform.rotation.z});
}

std::uint64_t MixVector(std::uint64_t hash, const Vector3& v) {
    return MixFloats(hash, {v.x, v.y, v.z});
}

}

std::uint64_t HashSnapshot(const RenderSnapshot& snapshot) {
    std::uint64_t hash = Mix(kFnvOffset, snapshot.tick);
    for (const EntitySnapshot& entity : snapshot.entities) {
        hash = Mix(hash, static_cast<std::uint32_t>(entity.id));
        hash = MixTransform(hash, entity.transform);
        hash = Mix(hash, static_cast<std::uint8_t>(entity.flags));
    }
    hash = MixTransform(hash, snapshot.camera.transform);
    hash = Mix(hash, static_cast<std::uint8_t>(snapshot.camera.flags));
    return Mix(hash, snapshot.simStateHash);
}

std::uint64_t HashRacerState(const BodyState& body) {
    std::uint64_t hash = kFnvOffset;
    for (const Vector3& v :
         {body.motion.velocity, body.motion.angularMomentum, body.facing, body.motion.basis.side}) {
        hash = MixVector(hash, v);
    }
    hash = MixFloats(hash, {body.turnRadius, body.thrust, body.steering.gain, body.steering.slip});
    for (const int value : {body.motion.contactCount, body.impulses.yawMs, body.impulses.pitchMs,
                            body.impulses.rollMs, body.airborneMs}) {
        hash = Mix(hash, static_cast<std::uint32_t>(value));
    }
    const auto flags = static_cast<std::uint8_t>((body.steering.isSteering ? 1U : 0U) |
                                                 (body.hasWallYaw ? 2U : 0U));
    return Mix(hash, flags);
}

std::vector<std::byte> EncodeSnapshotHashes(std::span<const std::uint64_t> hashes) {
    std::vector<std::byte> out;
    AppendHeader(out, kMagic, kVersion);
    for (const std::uint64_t hash : hashes) {
        AppendU64(out, hash);
    }
    AppendChecksum(out);
    return out;
}

Result<std::vector<std::uint64_t>> DecodeSnapshotHashes(std::span<const std::byte> bytes) {
    Result<assets::ByteReader> reader = OpenChecked(bytes, kMagic, kVersion, "snapshot hashes");
    if (!reader) {
        return std::unexpected(std::move(reader.error()));
    }
    if (reader->Remaining() % kHashBytes != 0) {
        return Fail(ErrorCode::InvalidData, "snapshot hashes end mid-hash");
    }
    std::vector<std::uint64_t> hashes;
    while (reader->Remaining() != 0) {
        hashes.push_back(*ReadU64(*reader));
    }
    return hashes;
}

}
