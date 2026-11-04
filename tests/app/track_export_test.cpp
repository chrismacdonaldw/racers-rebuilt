#include "app/track_export.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "app/track_info.hpp"
#include "assets/archive_survey.hpp"
#include "assets/gltf_data.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

std::vector<std::byte> ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<char> text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const std::span<const std::byte> bytes = std::as_bytes(std::span(text));
    return {bytes.begin(), bytes.end()};
}

std::string ReadText(const std::filesystem::path& path) {
    const std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

std::vector<std::string_view> MissingNodes(const std::filesystem::path& glb,
                                           std::span<const std::string_view> wanted) {
    const Result<assets::GltfData> gltf = assets::ParseGlb(ReadFile(glb));
    REQUIRE(gltf.has_value());
    std::set<std::string, std::less<>> names;
    for (const cgltf_node& node : std::span((*gltf)->nodes, (*gltf)->nodes_count)) {
        names.emplace(node.name != nullptr ? node.name : "");
    }
    std::vector<std::string_view> missing;
    std::ranges::copy_if(wanted, std::back_inserter(missing),
                         [&](std::string_view name) { return !names.contains(name); });
    return missing;
}

TEST_CASE("a retail track exports its worlds, collision and markers") {
    std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;

    REQUIRE(ExportTrack(*vfs, "racec0r0", folder.Path()).has_value());

    constexpr std::array<std::string_view, 5> kWanted{"collision", "start.6", "gate.01",
                                                      "route.1.F.0", "brick.white.1"};
    CHECK(MissingNodes(folder.Path() / "track.glb", kWanted).empty());
}

TEST_CASE("an exported track's info reads back with the default laps") {
    std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;
    REQUIRE(ExportTrack(*vfs, "RACEC0R0", folder.Path()).has_value());

    const Result<TrackInfo> info =
        ParseTrackInfo(ReadText(folder.Path() / "track.toml"), "RACEC0R0", "track.toml");

    REQUIRE(info.has_value());
    CHECK(info->name == "RACEC0R0");
    CHECK(info->lapCount == 3);
}

TEST_CASE("exporting a folder that isn't a race track fails") {
    const assets::Vfs vfs;
    const test::TempFolder folder;

    CHECK_FALSE(ExportTrack(vfs, "NOSUCH", folder.Path()).has_value());
    CHECK_FALSE(std::filesystem::exists(folder.Path() / "track.glb"));
}

}
}
