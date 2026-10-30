#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "archive_survey.hpp"
#include "assets/animation_database.hpp"
#include "assets/champion_database.hpp"
#include "assets/chassis_database.hpp"
#include "assets/driver_database.hpp"
#include "assets/game_path.hpp"
#include "assets/jam_archive.hpp"
#include "assets/skeleton_database.hpp"
#include "assets/vfs.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kCommon = "GAMEDATA/COMMON";

template <typename Parse>
auto ReadCommon(const Vfs& vfs, std::string_view file, Parse parse) {
    const Result<FileData> data = vfs.Read(std::format("{}/{}", kCommon, file));
    REQUIRE(data.has_value());
    auto parsed = parse(data->Bytes());
    REQUIRE(parsed.has_value());
    return std::move(*parsed);
}

template <typename Entry>
bool Names(std::span<const Entry> entries, std::string_view name) {
    return std::ranges::any_of(entries, [&](const Entry& entry) {
        return ToUpperAscii(entry.name) == ToUpperAscii(name);
    });
}

TEST_CASE("every car table, skeleton and animation file in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    const auto survey = [&](std::string_view extension, auto parse) {
        return test::SurveyLayer(
            *archive, [&](std::string_view name) { return test::Extension(name) == extension; },
            [&](std::span<const std::byte> bytes) {
                return parse(bytes).transform([](auto&&) {});
            });
    };
    test::CheckSurvey(survey("DDB", ParseDrivers), 2);
    test::CheckSurvey(survey("CCB", ParseChampions), 2);
    test::CheckSurvey(survey("CMB", ParseChassis), 2);
    test::CheckSurvey(survey("SDB", ParseSkeleton), 417);
    test::CheckSurvey(survey("ADB", ParseAnimations), 421);
}

template <typename Asset>
Result<Asset> ReadAsset(const JamArchive& archive, EntryInfo info,
                        Result<Asset> (*parse)(std::span<const std::byte>)) {
    return archive.Read(info).and_then(
        [parse](const FileData& data) { return parse(data.Bytes()); });
}

Result<void> CheckTrackCount(const JamArchive& archive, EntryInfo skeletonFile,
                             EntryInfo animationFile) {
    Result<SkeletonAsset> skeleton = ReadAsset(archive, skeletonFile, ParseSkeleton);
    Result<AnimationAsset> animations = skeleton.and_then(
        [&](const SkeletonAsset&) { return ReadAsset(archive, animationFile, ParseAnimations); });
    if (!animations) {
        return std::unexpected(std::move(animations.error()));
    }
    const std::size_t expected = animations->parts.size() * skeleton->nodes.size();
    if (animations->tracks.size() != expected) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} tracks, expected {}", animations->tracks.size(), expected));
    }
    return {};
}

TEST_CASE("every retail skeleton's animations hold one track per node per part") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    test::Survey pairs;
    for (const IndexEntry& entry : archive->Index().Entries()) {
        if (test::Extension(entry.path) != "ADB") {
            continue;
        }
        std::string skeletonPath = entry.path;
        skeletonPath.replace(skeletonPath.size() - 3, 3, "SDB");
        const std::optional<EntryInfo> skeleton = archive->Index().Find(skeletonPath);
        if (!skeleton) {
            continue;
        }
        ++pairs.fileCount;
        const Result<void> checked = CheckTrackCount(*archive, *skeleton, entry.info);
        if (!checked) {
            pairs.failures.push_back(std::format("{}: {}", entry.path, checked.error().message));
        }
    }
    test::CheckSurvey(pairs, 417);
}

template <typename Entry, typename Link>
std::vector<std::string> MissingLinks(std::span<const Entry> entries, std::span<const Link> targets,
                                      std::string Entry::* name) {
    std::vector<std::string> missing;
    for (const Entry& entry : entries) {
        if (!Names(targets, entry.*name)) {
            missing.push_back(entry.name);
        }
    }
    return missing;
}

TEST_CASE("every retail driver names a champion and every champion a chassis") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const std::vector<DriverEntry> drivers = ReadCommon(*vfs, "DRIVERS.DDB", ParseDrivers);
    const std::vector<ChampionEntry> champions = ReadCommon(*vfs, "CHAMPS.CCB", ParseChampions);
    const std::vector<ChassisEntry> chassis = ReadCommon(*vfs, "CHASSIS.CMB", ParseChassis);

    CHECK(std::array{drivers.size(), champions.size(), chassis.size()} ==
          std::array<std::size_t, 3>{24, 20, 24});
    CHECK(MissingLinks<DriverEntry, ChampionEntry>(drivers, champions, &DriverEntry::champion)
              .empty());
    CHECK(MissingLinks<ChampionEntry, ChassisEntry>(champions, chassis, &ChampionEntry::chassis)
              .empty());
    CHECK(std::ranges::all_of(
        champions, [](const ChampionEntry& champion) { return champion.mass == 4500.0F; }));
}

}
}
