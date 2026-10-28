#pragma once

#include <filesystem>
#include <fstream>

#include "assets/jam_builder.hpp"

namespace rr::test {

inline void WriteInstall(const std::filesystem::path& folder) {
    std::filesystem::create_directories(folder);
    std::ofstream(folder / "LEGO.JAM", std::ios::binary) << AsText(
        JamBuilder::Build({{.path = "GAMEDATA/COMMON/A.TXT", .contents = "a"}}, {"MENUDATA"}));
}

}
