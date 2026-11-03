#include "settings/progress.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "settings/toml_file.hpp"
#include "settings/trophy_table.hpp"

namespace rr::settings {
namespace {

constexpr std::string_view kHeader =
    "# racers-rebuilt progress. Safe to edit while the game is closed.\n"
    "#\n"
    "# unlocked_circuits  circuits opened by a gold or silver, by CRB name (c0 is always open)\n"
    "# part_sets          CRB part sets won with a gold\n"
    "# beaten_records     time trial races, 0 to 11, whose record was beaten\n"
    "# [trophies]         best trophy per circuit: gold, silver or bronze\n"
    "# [best_times.TRACK] lap and race records per track folder, in milliseconds\n"
    "\n";
constexpr std::string_view kUnlockedCircuits = "unlocked_circuits";
constexpr std::string_view kPartSets = "part_sets";
constexpr std::string_view kBeatenRecords = "beaten_records";
constexpr std::string_view kTrophies = "trophies";
constexpr std::string_view kBestTimes = "best_times";
constexpr std::string_view kLap = "lap";
constexpr std::string_view kRace = "race";
constexpr std::string_view kMs = "ms";
constexpr std::string_view kHolder = "holder";
constexpr std::array kProgressKeys{kUnlockedCircuits, kPartSets, kBeatenRecords, kTrophies,
                                   kBestTimes};

class Reader {
public:
    explicit Reader(std::string fileName) : fileName_(std::move(fileName)) {}

    void Skip(std::string_view key) const {
        log::Warn("{}: {} doesn't parse, skipping it", fileName_, key);
    }

    template <typename T, typename Container>
    void ReadArray(const toml::table& root, std::string_view key, Container& out) const {
        const toml::node* node = root.get(key);
        if (node == nullptr) {
            return;
        }
        const toml::array* array = node->as_array();
        if (array == nullptr) {
            Skip(key);
            return;
        }
        for (const toml::node& item : *array) {
            if (const std::optional<T> value = item.value<T>()) {
                out.insert(*value);
            } else {
                Skip(key);
            }
        }
    }

    void ReadTrophies(const toml::table& root, Progress& out) const {
        if (const toml::table* trophies = root[kTrophies].as_table()) {
            out.trophies = ReadTrophyTable(*trophies, fileName_);
        }
    }

    void ReadBestTimes(const toml::table& root, Progress& out) const {
        const toml::table* tracks = root[kBestTimes].as_table();
        if (tracks == nullptr) {
            return;
        }
        for (const auto& [track, node] : *tracks) {
            const toml::table* records = node.as_table();
            if (records == nullptr) {
                Skip(track.str());
                continue;
            }
            out.bestTimes.emplace(track.str(), TrackRecords{.lap = ReadTime(*records, kLap),
                                                            .race = ReadTime(*records, kRace)});
        }
    }

private:
    [[nodiscard]] std::optional<BestTime> ReadTime(const toml::table& records,
                                                   std::string_view key) const {
        const toml::table* time = records[key].as_table();
        if (time == nullptr) {
            return std::nullopt;
        }
        const std::optional<std::int64_t> ms = (*time)[kMs].value<std::int64_t>();
        if (!ms || *ms <= 0) {
            Skip(key);
            return std::nullopt;
        }
        return BestTime{.ms = *ms, .holder = (*time)[kHolder].value_or(std::string{})};
    }

    std::string fileName_;
};

template <typename Container>
toml::array ToArray(const Container& values) {
    toml::array array;
    for (const auto& value : values) {
        array.push_back(value);
    }
    return array;
}

toml::table TimeTable(const BestTime& time) {
    return toml::table{{kMs, time.ms}, {kHolder, time.holder}};
}

toml::table ToToml(const Progress& progress) {
    toml::table bestTimes;
    for (const auto& [track, records] : progress.bestTimes) {
        toml::table times;
        if (records.lap) {
            times.insert(kLap, TimeTable(*records.lap));
        }
        if (records.race) {
            times.insert(kRace, TimeTable(*records.race));
        }
        bestTimes.insert(track, std::move(times));
    }
    return toml::table{{kUnlockedCircuits, ToArray(progress.unlockedCircuits)},
                       {kPartSets, ToArray(progress.partSets)},
                       {kBeatenRecords, ToArray(progress.beatenRecords)},
                       {kTrophies, TrophyTable(progress.trophies)},
                       {kBestTimes, std::move(bestTimes)}};
}

}

bool AwardTrophy(Trophies& trophies, const std::string& circuit, Trophy trophy) {
    const auto [held, isNew] = trophies.try_emplace(circuit, trophy);
    if (isNew || trophy >= held->second) {
        return isNew;
    }
    held->second = trophy;
    return true;
}

bool HasBeatenEveryRecord(const Progress& progress) {
    return std::ranges::all_of(
        std::views::iota(std::int32_t{0}, static_cast<std::int32_t>(kRecordRaces)),
        [&](std::int32_t record) { return progress.beatenRecords.contains(record); });
}

Result<Progress> LoadProgress(const std::filesystem::path& path) {
    return ReadTomlFile(path).transform([&](const TomlFile& file) {
        const Reader reader(Utf8FromPath(path));
        Progress progress;
        reader.ReadArray<std::string>(file.table, kUnlockedCircuits, progress.unlockedCircuits);
        reader.ReadArray<std::int32_t>(file.table, kPartSets, progress.partSets);
        reader.ReadArray<std::int32_t>(file.table, kBeatenRecords, progress.beatenRecords);
        reader.ReadTrophies(file.table, progress);
        reader.ReadBestTimes(file.table, progress);
        return progress;
    });
}

Result<void> SaveProgress(const Progress& progress, const std::filesystem::path& path) {
    return ReplaceTomlKeys(path, ToToml(progress), kProgressKeys, kHeader);
}

}
