#include "settings/racers.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <format>
#include <span>
#include <string_view>
#include <utility>

#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "settings/toml_file.hpp"
#include "settings/trophy_table.hpp"

namespace rr::settings {
namespace {

constexpr std::string_view kHeader =
    "# racers-rebuilt racers. Safe to edit while the game is closed.\n"
    "#\n"
    "# selected          the racer that races, counting from 0\n"
    "# [[racers]]        a name, driver parts by BODYPART.PCB index, a chassis and a car\n"
    "# car               the original's car data in hex, without its trailing zeros\n"
    "# [racers.trophies] best trophy per circuit won with this racer\n"
    "\n";
constexpr std::string_view kSelected = "selected";
constexpr std::string_view kRacers = "racers";
constexpr std::string_view kName = "name";
constexpr std::string_view kChassis = "chassis";
constexpr std::string_view kCar = "car";
constexpr std::string_view kTrophies = "trophies";
constexpr std::array kStoreKeys{kSelected, kRacers};
constexpr int kHexBase = 16;
constexpr std::size_t kHexDigits = 2;

struct LookField {
    std::string_view key;
    std::uint8_t assets::DriverLook::* field;
};

constexpr std::array kLookFields{
    LookField{.key = "face", .field = &assets::DriverLook::face},
    LookField{.key = "hat", .field = &assets::DriverLook::hat},
    LookField{.key = "legs", .field = &assets::DriverLook::legs},
    LookField{.key = "torso", .field = &assets::DriverLook::torso},
    LookField{.key = "expression", .field = &assets::DriverLook::expression}};

std::optional<assets::CarData> ParseCar(std::string_view hex) {
    if (hex.size() % kHexDigits != 0 || hex.size() > assets::kCarDataSize * kHexDigits) {
        return std::nullopt;
    }
    assets::CarData car{};
    for (std::size_t at = 0; at < hex.size(); at += kHexDigits) {
        std::uint8_t value = 0;
        const char* end = hex.data() + at + kHexDigits;
        const auto [last, error] = std::from_chars(hex.data() + at, end, value, kHexBase);
        if (error != std::errc{} || last != end) {
            return std::nullopt;
        }
        car.at(at / kHexDigits) = std::byte{value};
    }
    return car;
}

std::string CarHex(const assets::CarData& car) {
    std::size_t length = car.size();
    while (length > 0 && car.at(length - 1) == std::byte{0}) {
        --length;
    }
    std::string hex;
    for (const std::byte value : std::span(car).first(length)) {
        hex += std::format("{:02x}", std::to_integer<unsigned>(value));
    }
    return hex;
}

bool ReadLook(const toml::table& table, assets::DriverLook& look) {
    for (const LookField& field : kLookFields) {
        const std::optional<std::int64_t> value = table[field.key].value<std::int64_t>();
        if (!value || !std::in_range<std::uint8_t>(*value)) {
            return false;
        }
        look.*field.field = static_cast<std::uint8_t>(*value);
    }
    return true;
}

std::optional<Racer> ReadRacer(const toml::node& node, std::string_view fileName) {
    const toml::table* table = node.as_table();
    if (table == nullptr) {
        return std::nullopt;
    }
    std::optional<std::string> name = (*table)[kName].value<std::string>();
    std::optional<std::string> chassis = (*table)[kChassis].value<std::string>();
    const std::optional<std::string_view> hex = (*table)[kCar].value<std::string_view>();
    const std::optional<assets::CarData> car = hex ? ParseCar(*hex) : std::nullopt;
    Racer racer;
    if (!name || !chassis || !car || !ReadLook(*table, racer.look)) {
        return std::nullopt;
    }
    racer.name = std::move(*name);
    racer.chassis = std::move(*chassis);
    racer.car = *car;
    if (const toml::table* trophies = (*table)[kTrophies].as_table()) {
        racer.trophies = ReadTrophyTable(*trophies, fileName);
    }
    return racer;
}

Result<RacerStore> ReadStore(const toml::table& root, std::string_view fileName) {
    RacerStore store;
    const toml::array* racers = root[kRacers].as_array();
    if (racers == nullptr && root.contains(kRacers)) {
        return Fail(ErrorCode::InvalidData, std::format("{}: {} isn't a list", fileName, kRacers));
    }
    for (std::size_t index = 0; racers != nullptr && index < racers->size(); ++index) {
        std::optional<Racer> racer = ReadRacer((*racers)[index], fileName);
        if (!racer) {
            return Fail(ErrorCode::InvalidData,
                        std::format("{}: racer {} doesn't parse", fileName, index));
        }
        store.racers.push_back(std::move(*racer));
    }
    const std::optional<std::int64_t> selected = root[kSelected].value<std::int64_t>();
    if (selected && *selected >= 0 && std::cmp_less(*selected, store.racers.size())) {
        store.selected = static_cast<std::size_t>(*selected);
    } else if (root.contains(kSelected)) {
        log::Warn("{}: {} doesn't name a racer, skipping it", fileName, kSelected);
    }
    return store;
}

toml::table RacerTable(const Racer& racer) {
    toml::table table{{kName, racer.name}};
    for (const LookField& field : kLookFields) {
        table.insert(field.key, static_cast<std::int64_t>(racer.look.*field.field));
    }
    table.insert(kChassis, racer.chassis);
    table.insert(kCar, CarHex(racer.car));
    table.insert(kTrophies, TrophyTable(racer.trophies));
    return table;
}

toml::table ToToml(const RacerStore& store) {
    toml::array racers;
    for (const Racer& racer : store.racers) {
        racers.push_back(RacerTable(racer));
    }
    toml::table root{{kRacers, std::move(racers)}};
    if (store.selected) {
        root.insert(kSelected, static_cast<std::int64_t>(*store.selected));
    }
    return root;
}

}

Trophies& TrophiesOf(Progress& progress, RacerStore& store) {
    return store.selected ? store.racers.at(*store.selected).trophies : progress.trophies;
}

Result<RacerStore> LoadRacers(const std::filesystem::path& path) {
    return ReadTomlFile(path).and_then(
        [&](const TomlFile& file) { return ReadStore(file.table, Utf8FromPath(path)); });
}

Result<RacerStore> ParseRacers(std::string_view text, std::string_view sourceName) {
    return ParseToml(text, sourceName).and_then([&](const toml::table& table) {
        return ReadStore(table, sourceName);
    });
}

Result<void> SaveRacers(const RacerStore& store, const std::filesystem::path& path) {
    return ReplaceTomlKeys(path, ToToml(store), kStoreKeys, kHeader);
}

}
