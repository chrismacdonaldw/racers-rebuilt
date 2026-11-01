#include "app/gameplay_fingerprint.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "mods/mod_list.hpp"
#include "settings/registry.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

TEST_CASE("the gameplay fingerprint names each enabled mod and its version") {
    const test::TempFolder folder;
    for (const char* id : {"faster", "quiet"}) {
        std::filesystem::create_directories(folder.Path() / id);
        std::ofstream(folder.Path() / id / "mod.toml", std::ios::binary)
            << "id = \"" << id << "\"\nname = \"" << id << "\"\nversion = \"1.2\"\n";
    }
    const mods::ModList mods = mods::ModList::Discover(folder.Path());
    settings::Registry registry;
    mods.DeclareSettings(registry);
    registry.Set(mods.All().back().EnabledSetting(), false);

    CHECK(GameplayFingerprint(registry, mods) == std::vector<std::string>{"mod:faster@1.2"});
}

}
}
