#include "assets/game_version.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "assets/string_table.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kMenuNamesPath = "MENUDATA/MENUNAME.SRF";
constexpr std::u16string_view kConsoleScreenName = u"loadnopt";

}

std::string_view NameOf(GameVersion version) {
    return kGameVersionNames[static_cast<std::size_t>(version)];
}

std::optional<GameVersion> ParseGameVersion(std::string_view name) {
    const auto found = std::ranges::find(kGameVersionNames, name);
    if (found == kGameVersionNames.end()) {
        return std::nullopt;
    }
    return static_cast<GameVersion>(std::distance(kGameVersionNames.begin(), found));
}

GameVersion IdentifyRelease(const VfsLayer& archive) {
    const std::optional<EntryInfo> entry = archive.Index().Find(kMenuNamesPath);
    if (!entry || entry->kind != EntryKind::File) {
        return GameVersion::Release1999;
    }
    const Result<std::vector<std::u16string>> names = archive.Read(*entry).and_then(
        [](const FileData& file) { return ParseStringTable(file.Bytes()); });
    const bool hasConsoleScreens = names && std::ranges::contains(*names, kConsoleScreenName);
    return hasConsoleScreens ? GameVersion::Release2001 : GameVersion::Release1999;
}

}
