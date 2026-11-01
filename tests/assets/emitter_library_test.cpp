#include "assets/emitter_library.hpp"

#include <doctest/doctest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "assets/jam_archive.hpp"
#include "assets/material_animation.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Floats;
using test::Ints;
using test::Join;

std::vector<std::byte> Float32(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    return Bytes({0x03, static_cast<std::uint8_t>(bits), static_cast<std::uint8_t>(bits >> 8U),
                  static_cast<std::uint8_t>(bits >> 16U), static_cast<std::uint8_t>(bits >> 24U)});
}

std::vector<std::byte> OnePoint() {
    return Join({Bytes({0x2B}), test::ListHeader(1), Floats({0, 0, 40}), Bytes({0x06})});
}

Result<std::vector<EmitterAsset>> Parse(std::initializer_list<std::vector<std::byte>> emitters) {
    return ParseEmitterLibrary(test::NamedLibrary(0x27, emitters));
}

TEST_CASE("an emitter library reads every emitter field") {
    const Result<std::vector<EmitterAsset>> emitters =
        Parse({test::NamedEntry(0x27, "grassemt",
                                Join({Bytes({0x28}),
                                      Floats({9}),
                                      Bytes({0x29}),
                                      Float32(0.25F),
                                      Bytes({0x2C}),
                                      Floats({2}),
                                      Bytes({0x2D}),
                                      Floats({3}),
                                      Bytes({0x31}),
                                      Floats({4}),
                                      Bytes({0x32}),
                                      Floats({5}),
                                      Bytes({0x2F}),
                                      Ints({200}),
                                      Bytes({0x30}),
                                      Ints({250}),
                                      Bytes({0x33}),
                                      Floats({100}),
                                      Bytes({0x2A}),
                                      Floats({0, 0, -52}),
                                      Bytes({0x2B}),
                                      test::ListHeader(2),
                                      Floats({1, 2, 3, -4, 5, 6}),
                                      Bytes({0x06, 0x35}),
                                      Ints({20}),
                                      Bytes({0x34}),
                                      test::Text("graspart")}))});

    REQUIRE(emitters.has_value());
    REQUIRE(emitters->size() == 1);
    const EmitterAsset& emitter = emitters->front();
    CHECK(emitter.name == "grassemt");
    CHECK(emitter.emitRate == doctest::Approx(9.0F));
    CHECK(emitter.emitChance == doctest::Approx(0.25F));
    CHECK(emitter.startSizeUp == doctest::Approx(2.0F));
    CHECK(emitter.startSizeForward == doctest::Approx(3.0F));
    CHECK(emitter.sizeGrowthUp == doctest::Approx(4.0F));
    CHECK(emitter.sizeGrowthForward == doctest::Approx(5.0F));
    CHECK(emitter.particleLife == Milliseconds{200});
    CHECK(emitter.duration == Milliseconds{250});
    CHECK(emitter.radius == doctest::Approx(100.0F));
    CHECK(emitter.acceleration == Vector3{.z = -52.0F});
    CHECK(emitter.points == std::vector<Vector3>{{1, 2, 3}, {-4, 5, 6}});
    CHECK(emitter.priority == 20);
    CHECK(emitter.material == "graspart");
    CHECK_FALSE(emitter.materialTrack.has_value());
}

TEST_CASE("an emitter with only its spawn velocities keeps the original's defaults") {
    const Result<std::vector<EmitterAsset>> emitters =
        Parse({test::NamedEntry(0x27, "dust", Join({OnePoint(), Bytes({0x2E}), Ints({1})}))});

    REQUIRE(emitters.has_value());
    const EmitterAsset& emitter = emitters->front();
    CHECK(emitter.startSizeUp == doctest::Approx(1.0F));
    CHECK(emitter.emitRate == doctest::Approx(0.0F));
    CHECK_FALSE(emitter.duration.has_value());
    CHECK(emitter.materialTrack == 1U);
    CHECK_FALSE(emitter.material.has_value());
}

TEST_CASE("an emitter that runs until stopped has no duration") {
    const Result<std::vector<EmitterAsset>> emitters = Parse(
        {test::NamedEntry(0x27, "smoke", Join({OnePoint(), Bytes({0x30, 0x0D, 0xFF, 0xFF})}))});

    REQUIRE(emitters.has_value());
    CHECK_FALSE(emitters->front().duration.has_value());
}

TEST_CASE("an empty emitter library parses") {
    const Result<std::vector<EmitterAsset>> emitters = Parse({});

    REQUIRE(emitters.has_value());
    CHECK(emitters->empty());
}

TEST_CASE("an emitter library rejects what the original can't run") {
    CHECK_FALSE(Parse({test::NamedEntry(0x27, "dust", {})}).has_value());
    CHECK_FALSE(Parse({test::NamedEntry(0x27, "toolongname", OnePoint())}).has_value());
    CHECK_FALSE(
        Parse({test::NamedEntry(0x27, "dust", Join({OnePoint(), Bytes({0x36}), Ints({1})}))})
            .has_value());
    CHECK_FALSE(
        Parse({test::NamedEntry(0x27, "dust", Join({OnePoint(), Bytes({0x28}), Ints({9})}))})
            .has_value());
    CHECK_FALSE(Parse({test::NamedEntry(0x27, "dust", OnePoint()),
                       test::NamedEntry(0x27, "dust", OnePoint())})
                    .has_value());
}

std::size_t PairedTrackCount(const JamArchive& archive, const IndexEntry& library) {
    const std::string_view folder = library.Parent();
    const std::string_view track = folder.substr(folder.rfind('/') + 1);
    const std::string_view path = library.path;
    const std::string paired =
        track.starts_with("RACE")
            ? std::format("{}/EMIT{}.MAB", folder, track.substr(std::string_view{"RACE"}.size()))
            : std::format("{}.MAB", path.substr(0, path.rfind('.')));
    const Result<MaterialAnimationAsset> animation =
        test::ReadRetailFile(archive, paired).and_then([](const FileData& file) {
            return ParseMaterialAnimation(file.Bytes());
        });
    REQUIRE_MESSAGE(animation.has_value(), paired);
    return animation->tracks.size();
}

struct LibraryCounts {
    std::size_t emitters = 0;
    std::size_t trackEmitters = 0;
};

void CountLibrary(const JamArchive& archive, const IndexEntry& library, LibraryCounts& counts) {
    CAPTURE(library.path);
    const Result<std::vector<EmitterAsset>> emitters =
        test::ParseEntry(archive, library, ParseEmitterLibrary);
    REQUIRE(emitters.has_value());
    counts.emitters += emitters->size();
    for (const EmitterAsset& emitter : *emitters) {
        if (emitter.materialTrack) {
            ++counts.trackEmitters;
            CHECK(*emitter.materialTrack < PairedTrackCount(archive, library));
        }
    }
}

TEST_CASE("every emitter library in the retail game parses and uses its paired tracks") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    const std::vector<IndexEntry> libraries = test::FilesWithExtension(*archive, "EMB");
    LibraryCounts counts;
    for (const IndexEntry& library : libraries) {
        CountLibrary(*archive, library, counts);
    }
    CHECK(libraries.size() == 17);
    CHECK(counts.emitters == 50);
    CHECK(counts.trackEmitters == 15);
}

}
}
