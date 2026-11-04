#include "app/racer/mod_driver_parts.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "assets/gltf_model.hpp"
#include "assets/merged_table.hpp"
#include "core/log.hpp"
#include "settings/toml_file.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPartFolder = "MENUDATA/PARTDB";
constexpr std::string_view kName = "name";
constexpr std::array<std::string_view, kFaceExpressions> kExpressionKeys{
    "default", "angry", "blink", "happy", "sad", "surprised"};
constexpr std::size_t kChannels = 3;

class Entry {
public:
    Entry(const toml::table& table, std::string_view source, std::string_view kind,
          std::size_t index)
        : table_(table), source_(source), kind_(kind), index_(index) {}

    [[nodiscard]] std::optional<std::string> Text(std::string_view key) const {
        const toml::node* node = Take(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        std::optional<std::string> text = node->value<std::string>();
        if (!text || text->empty()) {
            Warn(std::format("{} isn't text", key));
            return std::nullopt;
        }
        return text;
    }

    [[nodiscard]] std::optional<std::string> Required(std::string_view key) const {
        std::optional<std::string> text = Text(key);
        if (!text && !Has(key)) {
            Warn(std::format("needs {}", key));
        }
        return text;
    }

    [[nodiscard]] std::optional<bool> Flag(std::string_view key) const {
        const toml::node* node = Take(key);
        if (node == nullptr) {
            return false;
        }
        const std::optional<bool> flag = node->value<bool>();
        if (!flag) {
            Warn(std::format("{} isn't true or false", key));
        }
        return flag;
    }

    [[nodiscard]] std::optional<Rgba8> Colour(std::string_view key) const {
        const toml::node* node = Take(key);
        const toml::array* channels = node == nullptr ? nullptr : node->as_array();
        Rgba8 rgba = kOpaqueWhite;
        const bool isValid =
            channels != nullptr && channels->size() == kChannels &&
            std::ranges::all_of(std::views::iota(std::size_t{0}, kChannels), [&](std::size_t i) {
                const std::optional<std::int64_t> value = channels->get(i)->value<std::int64_t>();
                if (!value || !std::in_range<std::uint8_t>(*value)) {
                    return false;
                }
                rgba.at(i) = static_cast<std::uint8_t>(*value);
                return true;
            });
        if (!isValid) {
            Warn(std::format("{} needs three numbers from 0 to 255", key));
            return std::nullopt;
        }
        return rgba;
    }

    [[nodiscard]] bool Has(std::string_view key) const { return table_.get(key) != nullptr; }

    void WarnUnknownKeys() const {
        for (const auto& [key, node] : table_) {
            if (!std::ranges::contains(seen_, key.str())) {
                log::Warn("{}: [[{}]] {} has no key {}, ignoring it", source_, kind_, index_ + 1,
                          key.str());
            }
        }
    }

private:
    void Warn(std::string_view problem) const {
        log::Warn("{}: [[{}]] {} {}, skipping it", source_, kind_, index_ + 1, problem);
    }

    const toml::node* Take(std::string_view key) const {
        seen_.emplace_back(key);
        return table_.get(key);
    }

    const toml::table& table_;
    std::string_view source_;
    std::string_view kind_;
    std::size_t index_;
    mutable std::vector<std::string_view> seen_;
};

std::optional<ModHat> ReadHat(const Entry& entry) {
    std::optional<std::string> name = entry.Required(kName);
    std::optional<std::string> model = entry.Required("model");
    if (!name || !model) {
        return std::nullopt;
    }
    return ModHat{.name = std::move(*name), .model = std::move(*model)};
}

std::optional<ModFace> ReadFace(const Entry& entry) {
    std::optional<std::string> name = entry.Required(kName);
    ModFace face;
    bool isValid = name.has_value();
    for (std::size_t expression = 0; expression < kFaceExpressions; ++expression) {
        const std::string_view key = kExpressionKeys.at(expression);
        std::optional<std::string> image = expression == 0 ? entry.Required(key) : entry.Text(key);
        const bool isRead = image.has_value() || (expression != 0 && !entry.Has(key));
        isValid = isValid && isRead;
        face.images.at(expression) = std::move(image).value_or(std::string());
    }
    if (!name || !isValid) {
        return std::nullopt;
    }
    face.name = std::move(*name);
    return face;
}

std::optional<ModDress> ReadDress(const Entry& entry, std::string_view variantKey) {
    std::optional<std::string> name = entry.Required(kName);
    std::optional<std::string> texture = entry.Required("texture");
    const std::optional<bool> isVariant = entry.Flag(variantKey);
    if (!name || !texture || !isVariant) {
        return std::nullopt;
    }
    return ModDress{
        .name = std::move(*name), .texture = std::move(*texture), .variant = *isVariant ? 1 : 0};
}

std::optional<ModColour> ReadColour(const Entry& entry) {
    std::optional<std::string> name = entry.Required(kName);
    const std::optional<Rgba8> rgba = entry.Colour("rgb");
    if (!name || !rgba) {
        return std::nullopt;
    }
    return ModColour{.name = std::move(*name), .rgba = *rgba};
}

template <typename Part, typename Read>
std::vector<Part> ReadList(const toml::table& root, std::string_view kind, std::string_view source,
                           Read read) {
    std::vector<Part> parts;
    const toml::node* node = root.get(kind);
    if (node == nullptr) {
        return parts;
    }
    const toml::array* entries = node->as_array();
    if (entries == nullptr) {
        log::Warn("{}: {} isn't a list of [[{}]] entries, ignoring it", source, kind, kind);
        return parts;
    }
    for (std::size_t index = 0; index < entries->size(); ++index) {
        const toml::table* table = entries->get(index)->as_table();
        if (table == nullptr) {
            log::Warn("{}: {} {} isn't a table, skipping it", source, kind, index + 1);
            continue;
        }
        const Entry entry(*table, source, kind, index);
        if (std::optional<Part> part = read(entry)) {
            entry.WarnUnknownKeys();
            parts.push_back(std::move(*part));
        }
    }
    return parts;
}

template <auto List>
auto Merged(std::vector<ModDriverParts>& layers) {
    using Parts = std::remove_cvref_t<decltype(layers.front().*List)>;
    std::vector<Parts> tables;
    tables.reserve(layers.size());
    for (ModDriverParts& layer : layers) {
        tables.push_back(std::move(layer.*List));
    }
    return assets::MergeNamed(std::move(tables));
}

}

Result<ModDriverParts> ParseModDriverParts(std::string_view text, std::string_view source) {
    return settings::ParseToml(text, source).transform([&](const toml::table& root) {
        constexpr std::array<std::string_view, 5> kKinds{"hat", "face", "torso", "legs", "colour"};
        for (const auto& [key, node] : root) {
            if (!std::ranges::contains(kKinds, key.str())) {
                log::Warn("{}: there are no [[{}]] parts, ignoring them", source, key.str());
            }
        }
        return ModDriverParts{
            .hats = ReadList<ModHat>(root, "hat", source, ReadHat),
            .faces = ReadList<ModFace>(root, "face", source, ReadFace),
            .torsos = ReadList<ModDress>(
                root, "torso", source, [](const Entry& entry) { return ReadDress(entry, "hook"); }),
            .legs = ReadList<ModDress>(root, "legs", source,
                                       [](const Entry& entry) { return ReadDress(entry, "peg"); }),
            .colours = ReadList<ModColour>(root, "colour", source, ReadColour),
        };
    });
}

ModDriverParts LoadModDriverParts(const assets::Vfs& vfs) {
    Result<std::vector<assets::FileData>> files = vfs.ReadEveryLayer(kModPartsFile);
    if (!files) {
        log::Warn("{}, mods add no driver parts", files.error().message);
        return {};
    }
    std::vector<ModDriverParts> layers;
    for (const assets::FileData& file : *files) {
        const std::span<const std::byte> bytes = file.Bytes();
        Result<ModDriverParts> parts = ParseModDriverParts(
            std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
            kModPartsFile);
        if (parts) {
            layers.push_back(std::move(*parts));
        } else {
            log::Warn("{}, skipping a mod's driver parts", parts.error().message);
        }
    }
    return {.hats = Merged<&ModDriverParts::hats>(layers),
            .faces = Merged<&ModDriverParts::faces>(layers),
            .torsos = Merged<&ModDriverParts::torsos>(layers),
            .legs = Merged<&ModDriverParts::legs>(layers),
            .colours = Merged<&ModDriverParts::colours>(layers)};
}

assets::DriverPartCatalog ModPartCatalog(const ModDriverParts& parts) {
    assets::DriverPartCatalog catalog;
    for (const ModHat& hat : parts.hats) {
        catalog.hats.push_back({.name = hat.name});
        catalog.headParts.push_back(hat.name);
    }
    for (const ModFace& face : parts.faces) {
        catalog.faces.parts.push_back({.name = face.name});
    }
    for (const ModDress& torso : parts.torsos) {
        catalog.torsos.parts.push_back({.name = torso.name, .variant = torso.variant});
    }
    for (const ModDress& legs : parts.legs) {
        catalog.legs.parts.push_back({.name = legs.name, .variant = legs.variant});
    }
    return catalog;
}

assets::PartLibrary ModHeadParts(const assets::Vfs& vfs, const ModDriverParts& parts) {
    assets::PartLibrary library;
    for (const ModHat& hat : parts.hats) {
        const std::string path = assets::JoinGamePath(kPartFolder, hat.model);
        const Result<assets::GltfModel> model = vfs.Read(path).and_then(
            [](const assets::FileData& data) { return assets::ImportGltfModel(data.Bytes()); });
        if (model) {
            assets::AddLibraryPart(library, hat.name, model->geometry);
        } else {
            log::Warn("{}: {}, skipping hat {}", path, model.error().message, hat.name);
        }
    }
    return library;
}

}
