#include "app/mod_content.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/folder_layer.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

TEST_CASE("mod checks name a prebuilt driver whose champion is missing") {
    std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder mod;
    std::filesystem::create_directories(mod.Path() / "GAMEDATA" / "COMMON");
    std::ofstream(mod.Path() / "GAMEDATA" / "COMMON" / "DRIVERS.TOML", std::ios::binary)
        << "[[driver]]\ncode = \"CR\"\nchampion = \"NOCAR\"\n";
    Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(mod.Path());
    REQUIRE(layer.has_value());
    vfs->Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));

    const std::vector<std::string> problems = ModContentProblems(*vfs);

    CHECK(std::ranges::any_of(problems, [](const std::string& problem) {
        return problem.starts_with("GAMEDATA/COMMON/DRIVERS.TOML: driver CR: ");
    }));
}

}
}
