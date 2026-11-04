#include "app/texture_keys.hpp"

#include <algorithm>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "app/image_dump.hpp"
#include "assets/font_list.hpp"
#include "assets/game_path.hpp"
#include "assets/image_list.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

struct KeyedName {
    std::string fileName;
    std::optional<assets::ColorKey> key;
};

template <typename Entry>
Result<std::vector<KeyedName>> NamesOf(Result<std::vector<Entry>> entries) {
    return entries.transform([](const std::vector<Entry>& list) {
        std::vector<KeyedName> names;
        names.reserve(list.size());
        for (const Entry& entry : list) {
            names.push_back(
                {.fileName = assets::ToUpperAscii(entry.FileName()), .key = entry.colorKey});
        }
        return names;
    });
}

std::optional<Result<std::vector<KeyedName>>> ParseTable(std::string_view gamePath,
                                                         std::span<const std::byte> bytes) {
    if (gamePath.ends_with(".TDB")) {
        return NamesOf(assets::ParseTextureDatabase(bytes));
    }
    if (gamePath.ends_with(".IDB")) {
        return NamesOf(assets::ParseImageList(bytes));
    }
    if (gamePath.ends_with(".FDB")) {
        return NamesOf(assets::ParseFontList(bytes));
    }
    return std::nullopt;
}

void AddKeys(std::span<const KeyedName> names, ColorKeys& keys) {
    for (const KeyedName& name : names) {
        if (!name.key) {
            continue;
        }
        const auto [found, isNew] = keys.byName.try_emplace(name.fileName, *name.key);
        if (!isNew && found->second != *name.key &&
            !std::ranges::contains(keys.conflicts, name.fileName)) {
            keys.conflicts.push_back(name.fileName);
        }
    }
}

}

Result<ColorKeys> CollectColorKeys(const assets::Vfs& vfs, std::size_t layer) {
    ColorKeys keys;
    Result<void> walked = ForEachGameFile(vfs, [&](const std::string& gamePath) -> Result<void> {
        const std::string upper = assets::ToUpperAscii(gamePath);
        if (!upper.ends_with("DB")) {
            return {};
        }
        Result<std::optional<assets::FileData>> data = vfs.ReadLayer(layer, gamePath);
        if (!data || !*data) {
            return {};
        }
        std::optional<Result<std::vector<KeyedName>>> names = ParseTable(upper, (*data)->Bytes());
        if (names && !*names) {
            log::Warn("{} skipped for colour keys: {}", gamePath, names->error().message);
        } else if (names) {
            AddKeys(**names, keys);
        }
        return {};
    });
    return walked.transform([&] { return std::move(keys); });
}

}
