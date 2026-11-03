#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "assets/game_version.hpp"
#include "assets/jam_builder.hpp"

namespace rr::test {

[[nodiscard]] inline std::string StringTableBytes(std::initializer_list<std::string_view> names) {
    std::vector<std::uint16_t> offsets;
    std::vector<std::uint16_t> data;
    for (const std::string_view name : names) {
        offsets.push_back(static_cast<std::uint16_t>(data.size()));
        data.insert(data.end(), name.begin(), name.end());
        data.push_back(0);
    }
    std::vector<std::uint16_t> units{static_cast<std::uint16_t>(offsets.size()),
                                     static_cast<std::uint16_t>(data.size())};
    units.insert(units.end(), offsets.begin(), offsets.end());
    units.insert(units.end(), data.begin(), data.end());
    std::string bytes;
    for (const std::uint16_t unit : units) {
        bytes.push_back(static_cast<char>(unit & 0xFFU));
        bytes.push_back(static_cast<char>(unit >> 8U));
    }
    return bytes;
}

[[nodiscard]] inline std::vector<JamFile> ReleaseFiles(assets::GameVersion version) {
    const std::string menuNames = version == assets::GameVersion::Release2001
                                      ? StringTableBytes({"mainmenu", "loadnopt", "menufont"})
                                      : StringTableBytes({"mainmenu", "menufont"});
    return {{.path = "GAMEDATA/COMMON/A.TXT", .contents = "a"},
            {.path = "MENUDATA/MENUNAME.SRF", .contents = menuNames}};
}

inline void WriteInstall(const std::filesystem::path& folder,
                         assets::GameVersion version = assets::GameVersion::Release1999) {
    std::filesystem::create_directories(folder);
    std::ofstream(folder / "LEGO.JAM", std::ios::binary)
        << AsText(JamBuilder::Build(ReleaseFiles(version)));
}

}
