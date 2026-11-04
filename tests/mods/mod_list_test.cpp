#include "mods/mod_list.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "assets/jam_builder.hpp"
#include "mods/mod_check.hpp"
#include "temp_folder.hpp"

namespace rr::mods {
namespace {

class ModsFixture {
public:
    void AddMod(std::string_view id, int priority, bool isEnabled = true,
                std::string_view tables = "") {
        WriteText(std::filesystem::path(id) / "mod.toml",
                  std::format("id = \"{}\"\nname = \"{}\"\nversion = \"1.0\"\npriority = {}\n"
                              "enabled = {}\n{}",
                              id, id, priority, isEnabled, tables));
    }

    void WriteText(const std::filesystem::path& relative, std::string_view text) {
        const std::filesystem::path path = folder_.Path() / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << text;
    }

    [[nodiscard]] ModList Discover() const { return ModList::Discover(folder_.Path()); }

    static std::vector<std::string> Ids(const std::vector<const Mod*>& mods) {
        return mods | std::views::transform([](const Mod* mod) { return mod->Manifest().id; }) |
               std::ranges::to<std::vector>();
    }

    static std::vector<std::string> Ids(const ModList& mods) {
        return mods.All() |
               std::views::transform([](const Mod& mod) { return mod.Manifest().id; }) |
               std::ranges::to<std::vector>();
    }

protected:
    test::TempFolder folder_;
};

TEST_CASE("a missing mods folder has no mods") {
    const test::TempFolder folder;

    CHECK(ModList::Discover(folder.Path() / "mods").All().empty());
}

TEST_CASE_FIXTURE(ModsFixture, "mods are ordered by priority, then by id") {
    AddMod("low", -1);
    AddMod("beta", 3);
    AddMod("alpha", 3);
    AddMod("plain", 0);

    CHECK(Ids(Discover()) == std::vector<std::string>{"alpha", "beta", "plain", "low"});
}

TEST_CASE_FIXTURE(ModsFixture, "a broken or mismatched mod is skipped") {
    AddMod("good", 0);
    WriteText("no-manifest/files/x.txt", "x");
    WriteText("broken/mod.toml", "id = ");
    WriteText("renamed/mod.toml", "id = \"other\"\nname = \"Other\"\nversion = \"1\"\n");

    CHECK(Ids(Discover()) == std::vector<std::string>{"good"});
}

TEST_CASE_FIXTURE(ModsFixture, "each mod's enabled setting defaults from its manifest") {
    AddMod("on", 0, true);
    AddMod("off", 0, false);
    const ModList mods = Discover();
    settings::Registry registry;
    mods.DeclareSettings(registry);

    CHECK(Ids(mods.Enabled(registry)) == std::vector<std::string>{"on"});

    REQUIRE(registry.AssignText("mods.off.enabled", "true").has_value());
    REQUIRE(registry.AssignText("mods.on.enabled", "false").has_value());
    CHECK(Ids(mods.Enabled(registry)) == std::vector<std::string>{"off"});
}

TEST_CASE_FIXTURE(ModsFixture, "only enabled mods mount, the highest priority on top") {
    AddMod("high", 10);
    AddMod("low", 1);
    AddMod("off", 20, false);
    WriteText("high/files/GAMEDATA/A.TXT", "high");
    WriteText("low/files/GAMEDATA/A.TXT", "low");
    WriteText("low/files/GAMEDATA/B.TXT", "low");
    WriteText("off/files/GAMEDATA/A.TXT", "off");
    WriteText("off/files/GAMEDATA/C.TXT", "off");
    const ModList mods = Discover();
    settings::Registry registry;
    mods.DeclareSettings(registry);
    assets::Vfs vfs;

    mods.MountEnabled(vfs, registry);

    const auto text = [&](std::string_view path) {
        Result<assets::FileData> file = vfs.Read(path);
        REQUIRE(file.has_value());
        return std::string(test::AsText(file->Bytes()));
    };
    CHECK(text("GAMEDATA/A.TXT") == "high");
    CHECK(text("GAMEDATA/B.TXT") == "low");
    CHECK(vfs.Read("GAMEDATA/C.TXT").error().code == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(ModsFixture, "a required mod mounts below the mod that needs it") {
    AddMod("base", 10);
    AddMod("addon", 1, true, "[requires]\nbase = \"*\"\n");
    WriteText("base/files/GAMEDATA/A.TXT", "base");
    WriteText("addon/files/GAMEDATA/A.TXT", "addon");
    const ModList mods = Discover();
    settings::Registry registry;
    mods.DeclareSettings(registry);
    assets::Vfs vfs;

    mods.MountEnabled(vfs, registry);

    Result<assets::FileData> file = vfs.Read("GAMEDATA/A.TXT");
    REQUIRE(file.has_value());
    CHECK(test::AsText(file->Bytes()) == "addon");
    CHECK(Ids(mods.Enabled(registry)) == std::vector<std::string>{"addon", "base"});
}

TEST_CASE_FIXTURE(ModsFixture, "the mod check lists skipped mods and mods held back") {
    AddMod("addon", 0, true, "[requires]\nbase = \"*\"\n");
    AddMod("idle", 0, false, "[requires]\nbase = \"*\"\n");
    WriteText("broken/mod.toml", "id = ");
    const ModList mods = Discover();
    settings::Registry registry;
    mods.DeclareSettings(registry);

    const std::vector<std::string> problems = ModProblems(mods, registry);

    REQUIRE(problems.size() == 2);
    CHECK(problems[0].find("broken") != std::string::npos);
    CHECK(problems[1].ends_with("mod.toml: Needs base"));
    CHECK(problems[1].find("addon") != std::string::npos);
    CHECK_FALSE(CheckMods(mods, registry, {}).has_value());
}

TEST_CASE_FIXTURE(ModsFixture,
                  "turning on what is required changes only the toggles that are off") {
    AddMod("base", 0, false);
    AddMod("addon", 0, false, "[requires]\nbase = \"*\"\n");
    const ModList mods = Discover();
    settings::Registry registry;
    mods.DeclareSettings(registry);
    registry.Set(mods.All().front().EnabledSetting(), true);

    CHECK(mods.TurnOnRequired(registry) == std::vector<std::string>{"mods.base.enabled"});
    CHECK(mods.Toggles(registry) == std::vector<bool>{true, true});
}

}
}
