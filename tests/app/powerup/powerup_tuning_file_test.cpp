#include "app/powerup/powerup_tuning_file.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "assets/folder_layer.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

using sim::PowerupKind;

constexpr std::size_t kYellow = 3;

sim::PowerupTuning Applied(std::string_view text) {
    sim::PowerupTuning tuning;
    REQUIRE(ApplyPowerupTuning(text, "POWERUPS.TOML", tuning).has_value());
    return tuning;
}

bool IsOriginal(const sim::PowerupTuning& tuning) {
    return FormatPowerupTuning(tuning) == FormatPowerupTuning(sim::kOriginalPowerups);
}

class ModLayers {
public:
    void Add(std::string_view text) {
        const std::filesystem::path root = folder_.Path() / std::to_string(count_++);
        std::filesystem::create_directories(root / "GAMEDATA/COMMON");
        std::ofstream(root / "GAMEDATA/COMMON/POWERUPS.TOML", std::ios::binary) << text;
        Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(root);
        REQUIRE(layer.has_value());
        vfs_.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
    }

    [[nodiscard]] const assets::Vfs& Vfs() const { return vfs_; }

private:
    test::TempFolder folder_;
    assets::Vfs vfs_;
    int count_ = 0;
};

}

TEST_CASE("a power-up file changes only the keys it names") {
    const sim::PowerupTuning tuning = Applied("[missile]\narm_ms = [100, 200, 300]\nrange = 600\n");

    CHECK(tuning.missile.armTimes[0] == Milliseconds{100});
    CHECK(tuning.missile.armTimes[2] == Milliseconds{300});
    CHECK(tuning.missile.range == 600.0F);
    CHECK(tuning.missile.shot.speed == 170.0F);
    CHECK(tuning.oilSlick.lifetime == Milliseconds{10'000});
}

TEST_CASE("power-up times and spreads are clamped so nothing divides by zero or wraps") {
    const sim::PowerupTuning tuning = Applied(
        "[lightning]\nramp_ms = 0\nfade_ms = -5\n[warp]\nactive_ms = 0\n[dynamite]\nscatter = "
        "-3\n[missile]\narm_ms = [0, 0, 0]\n[oil_slick]\nlifetime_ms = 0\n");

    CHECK(tuning.lightning.rampTime == Milliseconds{1});
    CHECK(tuning.lightning.fadeTime == Milliseconds{1});
    CHECK(tuning.warp.activeTime == Milliseconds{1});
    CHECK(tuning.dynamite.scatter == 0);
    CHECK(tuning.missile.armTimes[1] == Milliseconds{1});
    CHECK(tuning.oilSlick.lifetime == Milliseconds{1});
}

TEST_CASE("unknown keys, wrong types and wrong-length lists leave the values alone") {
    CHECK(
        IsOriginal(Applied("[missile]\nrange = \"far\"\narm_ms = [1, 2]\nboom = 3\n"
                           "[rocket]\nspeed = 1.0\n[turbo]\nlevels = [\"weak\", \"fast\", "
                           "\"hover\", \"hover\"]\n[slots]\nyellow = [\"oil_slick\"]\n"
                           "purple = [\"curse\", \"curse\", \"curse\", \"curse\"]\n")));
}

TEST_CASE("a file that doesn't parse is an error") {
    sim::PowerupTuning tuning;

    CHECK_FALSE(ApplyPowerupTuning("[missile", "POWERUPS.TOML", tuning).has_value());
}

TEST_CASE("slots remap what a colour gives at each level") {
    const sim::PowerupTuning tuning =
        Applied("[slots]\nyellow = [\"oil_slick\", \"dynamite\", \"magnet\", \"lightning\"]\n");

    CHECK(tuning.slots[kYellow][3] == PowerupKind::Lightning);
    CHECK(tuning.slots[kYellow][0] == PowerupKind::OilSlick);
    CHECK(tuning.slots[0] == sim::kOriginalSlots[0]);
}

TEST_CASE("distances are read in units, cones and turbos by name") {
    const sim::PowerupTuning tuning = Applied(
        "[magnet]\ngrab_distance = 5\n[lightning]\nvictim_cone_max = 100.0\nvictim_cone_cosine "
        "= 0.25\n[turbo]\nlevels = [\"hover\", \"hover\", \"weak\", \"strong\"]\n");

    CHECK(tuning.magnet.grabDistanceSquared == 25.0F);
    CHECK(tuning.lightning.victimCone.maxDistanceSquared == 10'000.0F);
    CHECK(tuning.lightning.victimCone.minDistanceSquared == 9.0F);
    CHECK(tuning.lightning.victimCone.cosine == 0.25F);
    CHECK(tuning.turbo.levels[0] == sim::TurboLevel::Hover);
    CHECK(tuning.turbo.levels[3] == sim::TurboLevel::Strong);
}

TEST_CASE("the power-up dump lists every key and reads back as the original values") {
    const std::string dump = FormatPowerupTuning(sim::kOriginalPowerups);
    sim::PowerupTuning tuning = Applied(
        "[missile]\nrange = 50\n[slots]\nred = [\"curse\", "
        "\"curse\", \"curse\", \"curse\"]\n");

    REQUIRE(ApplyPowerupTuning(dump, "dump", tuning).has_value());

    CHECK(IsOriginal(tuning));
    CHECK(
        dump.contains("[missile]\nspeed = 170.0\ngravity = -32.176\nlifetime_ms = 5500\n"
                      "arm_ms = [933, 1200, 1333]\n"));
    CHECK(dump.contains("grab_distance = 9.0\n"));
    CHECK(dump.contains("yellow = [\"oil_slick\", \"dynamite\", \"magnet\", \"curse\"]\n"));
}

TEST_CASE("power-up files apply lowest layer first, and a broken one is skipped") {
    ModLayers layers;
    layers.Add("[missile]\nrange = 600\nspin_turns = 3\n");
    layers.Add("[missile");
    layers.Add("[missile]\nrange = 700\n");

    const sim::PowerupTuning tuning = LoadPowerupTuning(layers.Vfs());

    CHECK(tuning.missile.range == 700.0F);
    CHECK(tuning.missile.spinTurns == 3.0F);
}

TEST_CASE("without power-up files the values are the original ones") {
    CHECK(IsOriginal(LoadPowerupTuning(assets::Vfs{})));
    CHECK_FALSE(PowerupTuningFingerprint(sim::kOriginalPowerups).has_value());
}

}
