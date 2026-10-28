#include "assets/checkpoints.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "assets/jam_archive.hpp"
#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Floats;
using test::Ints;
using test::Join;

std::vector<std::byte> Entry(std::int16_t x, std::initializer_list<std::uint8_t> links) {
    return Join({Bytes({0x27, 0x05, 0x28}), Floats({-1, 0, 0, x}), Bytes({0x2A}), Floats({x, 5, 2}),
                 Bytes({0x29}), Ints(links), Bytes({0x06})});
}

std::vector<std::byte> File(std::initializer_list<std::vector<std::byte>> entries) {
    std::vector<std::byte> bytes =
        Join({Bytes({0x27}), test::ListHeader(static_cast<std::uint8_t>(entries.size()))});
    for (const std::vector<std::byte>& entry : entries) {
        bytes.insert(bytes.end(), entry.begin(), entry.end());
    }
    bytes.push_back(std::byte{0x06});
    return bytes;
}

TEST_CASE("a checkpoint file reads each gate's plane, position and links") {
    const Result<CheckpointsAsset> asset =
        ParseCpb(File({Entry(0, {1, 255, 255, 255}), Entry(10, {0, 255, 255, 255})}));

    REQUIRE(asset.has_value());
    REQUIRE(asset->checkpoints.size() == 2);
    const Checkpoint& second = asset->checkpoints[1];
    CHECK(second.normal == Vector3{-1, 0, 0});
    CHECK(second.distance == 10.0F);
    CHECK(second.position == Vector3{10, 5, 2});
    CHECK(second.links == std::array<std::uint8_t, 4>{0, 255, 255, 255});
}

TEST_CASE("a checkpoint file rejects links past the last checkpoint") {
    CHECK_FALSE(ParseCpb(File({Entry(0, {1, 255, 255, 255})})).has_value());
    CHECK_FALSE(ParseCpb(File({Entry(0, {255, 255, 255, 255})})).has_value());
    CHECK_FALSE(ParseCpb(File({Entry(0, {0, 3, 255, 255})})).has_value());
}

TEST_CASE("a checkpoint file rejects a checkpoint missing a field") {
    const std::vector<std::byte> noLinks = Join({Bytes({0x27, 0x05, 0x28}), Floats({-1, 0, 0, 0}),
                                                 Bytes({0x2A}), Floats({0, 0, 0}), Bytes({0x06})});

    CHECK_FALSE(ParseCpb(File({noLinks})).has_value());
}

TEST_CASE("every checkpoint file in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    std::size_t checkpointCount = 0;
    const test::Survey files = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "CPB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseCpb(bytes).transform([&](const CheckpointsAsset& asset) {
                checkpointCount += asset.checkpoints.size();
            });
        });
    MESSAGE("CPB: ", files.fileCount, " files, ", checkpointCount, " checkpoints");
    test::CheckSurvey(files, 13);
    CHECK(checkpointCount == 713);
}

}
}
