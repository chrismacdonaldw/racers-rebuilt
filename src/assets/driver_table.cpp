#include "assets/driver_table.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/toml_entries.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kDriversKey = "driver";
constexpr std::string_view kNoCharge = "none";
constexpr std::int64_t kMostChance = 255;
constexpr std::int64_t kMostChargeLevel = 3;

using UseChances = std::array<std::optional<std::uint8_t>, kBrickColourNames.size()>;

std::optional<BrickColour> ColourNamed(std::string_view name) {
    const auto found = std::ranges::find(kBrickColourNames, name);
    if (found == kBrickColourNames.end()) {
        return std::nullopt;
    }
    return static_cast<BrickColour>(found - kBrickColourNames.begin());
}

const toml::table* SubTable(TomlFields& fields, std::string_view key) {
    const toml::node* node = fields.Find(key);
    if (node == nullptr) {
        return nullptr;
    }
    const toml::table* table = node->as_table();
    if (table == nullptr) {
        fields.Complain(std::format("{} must be a table", key));
    }
    return table;
}

UseChances ReadChances(TomlFields& fields) {
    UseChances chances{};
    const toml::table* table = SubTable(fields, "use_chances");
    if (table == nullptr) {
        return chances;
    }
    TomlFields colours(*table);
    for (std::size_t colour = 0; colour < kBrickColourNames.size(); ++colour) {
        const std::optional<std::int64_t> chance =
            colours.Number(kBrickColourNames[colour], 0, kMostChance);
        if (chance) {
            chances[colour] = static_cast<std::uint8_t>(*chance);
        }
    }
    if (colours.Problem()) {
        fields.Complain(std::format("use_chances: {}", *colours.Problem()));
    }
    return chances;
}

std::optional<DriverCharge> ReadCharge(TomlFields& fields) {
    const toml::table* table = SubTable(fields, "charge");
    if (table == nullptr) {
        return std::nullopt;
    }
    TomlFields charge(*table);
    const std::string colour = charge.Text("colour").value_or(std::string(kNoCharge));
    const std::int64_t level = charge.Number("level", 0, kMostChargeLevel).value_or(0);
    const std::optional<BrickColour> brick = ColourNamed(colour);
    if (!brick && colour != kNoCharge) {
        charge.Complain("colour must be red, blue, green, yellow or none");
    }
    if (charge.Problem()) {
        fields.Complain(std::format("charge: {}", *charge.Problem()));
        return std::nullopt;
    }
    return DriverCharge{.colour = brick, .level = static_cast<std::int32_t>(level)};
}

Result<DriverPatch> ParseDriver(const toml::table& table) {
    TomlFields fields(table);
    DriverPatch patch{.code = ToUpperAscii(fields.Text("code").value_or(std::string())),
                      .name = fields.Text("name"),
                      .racer = fields.Text("racer"),
                      .champion = fields.Text("champion"),
                      .voice = fields.Text("voice"),
                      .useChances = ReadChances(fields),
                      .charge = ReadCharge(fields),
                      .startBoost = fields.Flag("start_boost")};
    if (fields.Problem()) {
        return Fail(ErrorCode::InvalidData, *fields.Problem());
    }
    if (patch.code.empty()) {
        return Fail(ErrorCode::InvalidData, "code must name the driver");
    }
    return patch;
}

std::vector<DriverEntry>::iterator FindCode(std::vector<DriverEntry>& drivers,
                                            std::string_view code) {
    const std::string wanted = ToUpperAscii(code);
    return std::ranges::find_if(
        drivers, [&](const DriverEntry& driver) { return ToUpperAscii(driver.name) == wanted; });
}

void Patch(const DriverPatch& patch, std::optional<std::int32_t> voiceBank, DriverEntry& driver) {
    if (patch.name) {
        driver.title = *patch.name;
    }
    if (patch.racer) {
        driver.racer = *patch.racer;
    }
    if (patch.champion) {
        driver.champion = *patch.champion;
    }
    driver.voiceBank = voiceBank.value_or(driver.voiceBank);
    for (std::size_t colour = 0; colour < patch.useChances.size(); ++colour) {
        if (patch.useChances[colour]) {
            driver.useChances[colour] = patch.useChances[colour];
        }
    }
    if (patch.charge) {
        driver.aiChargeColor =
            patch.charge->colour ? static_cast<std::int32_t>(*patch.charge->colour) + 1 : 0;
        driver.aiWhiteBrickTarget = patch.charge->level;
    }
    if (patch.startBoost) {
        driver.startBoost = patch.startBoost;
    }
}

Result<void> Apply(const DriverPatch& patch, std::vector<DriverEntry>& drivers) {
    std::optional<std::int32_t> voiceBank;
    if (patch.voice) {
        const auto voice = FindCode(drivers, *patch.voice);
        if (voice == drivers.end()) {
            return Fail(ErrorCode::NotFound, std::format("voice names no driver {}", *patch.voice));
        }
        voiceBank = voice->voiceBank;
    }
    auto driver = FindCode(drivers, patch.code);
    if (driver == drivers.end()) {
        if (!patch.name || !patch.racer) {
            return Fail(ErrorCode::InvalidData, "a new driver needs name and racer");
        }
        driver = drivers.insert(drivers.end(), DriverEntry{.name = patch.code, .nameIndex = -1});
    }
    Patch(patch, voiceBank, *driver);
    return {};
}

}

Result<std::vector<DriverPatch>> ParseDriverTable(std::string_view text,
                                                  std::string_view sourceName) {
    return ParseTomlEntries<DriverPatch>(text, sourceName, kDriversKey, ParseDriver);
}

Result<ModTables<DriverPatch>> ReadDriverTables(const Vfs& vfs) {
    return ReadModTables(vfs, kDriverTablePath, ParseDriverTable);
}

std::vector<std::string> ApplyDriverPatches(std::span<const DriverPatch> patches,
                                            std::vector<DriverEntry>& drivers) {
    std::vector<std::string> problems;
    for (const DriverPatch& patch : patches) {
        if (const Result<void> applied = Apply(patch, drivers); !applied) {
            problems.push_back(std::format("{}: driver {}: {}", kDriverTablePath, patch.code,
                                           applied.error().message));
        }
    }
    return problems;
}

}
