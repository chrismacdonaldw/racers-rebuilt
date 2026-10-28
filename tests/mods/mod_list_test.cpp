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
#include "temp_folder.hpp"

namespace rr::mods {
namespace {

class ModsFixture {
public:
    void AddMod(std::string_view id, int priority, bool isEnabled = true) {
        WriteText(std::filesystem::path(id) / "mod.toml",
                  std::format("id = \"{}\"\nname = \"{}\"\nversion = \"1.0\"\npriority = {}\n"
                              "enabled = {}\n",
                              id, id, priority, isEnabled));
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

}
}
