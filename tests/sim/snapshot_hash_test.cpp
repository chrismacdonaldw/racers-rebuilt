#include "sim/replay/snapshot_hash.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "sim/physics/plane_world.hpp"

namespace rr::sim {
namespace {

RenderSnapshot Sample() {
    return {.tick = 7,
            .entities = {{.id = EntityId{1}, .transform = {.position = {.x = 1.0F, .y = 2.0F}}}},
            .camera = {.transform = {.position = {.z = 5.0F}}}};
}

TEST_CASE("any change to a snapshot changes its hash") {
    const RenderSnapshot base = Sample();
    RenderSnapshot nudged = base;
    nudged.entities[0].transform.position.x = std::nextafter(1.0F, 2.0F);
    RenderSnapshot later = base;
    later.tick = 8;
    RenderSnapshot teleported = base;
    teleported.camera.flags = SnapshotFlags::Teleported;
    RenderSnapshot drifted = base;
    drifted.simStateHash = 1;

    CHECK(HashSnapshot(base) == HashSnapshot(Sample()));
    CHECK(HashSnapshot(nudged) != HashSnapshot(base));
    CHECK(HashSnapshot(later) != HashSnapshot(base));
    CHECK(HashSnapshot(teleported) != HashSnapshot(base));
    CHECK(HashSnapshot(drifted) != HashSnapshot(base));
}

TEST_CASE("racer state hashes see the driver's steering") {
    const test::PlaneWorld world = test::FlatGround();
    Racer straight{{}, {}, {}};
    Racer steering{{}, {}, {}};

    CHECK(HashRacerState(straight) == HashRacerState(steering));
    straight.Tick(Milliseconds{12}, {}, world);
    steering.Tick(Milliseconds{12}, {.isLeftHeld = true}, world);

    CHECK(HashRacerState(steering) != HashRacerState(straight));
}

TEST_CASE("snapshot hashes survive encoding and reject damage") {
    const std::array<std::uint64_t, 3> hashes{1, 0xFFFF'FFFF'FFFF'FFFFULL, 42};
    std::vector<std::byte> bytes = EncodeSnapshotHashes(hashes);

    const Result<std::vector<std::uint64_t>> decoded = DecodeSnapshotHashes(bytes);

    REQUIRE(decoded.has_value());
    CHECK(*decoded == std::vector<std::uint64_t>(hashes.begin(), hashes.end()));
    bytes[8] ^= std::byte{1};
    CHECK_FALSE(DecodeSnapshotHashes(bytes).has_value());
}

}
}
