#include "app/save_import.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "assets/game_path.hpp"
#include "core/log.hpp"
#include "core/utf16_text.hpp"
#include "core/utf8_path.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kSaveFolder = "Save";
constexpr std::string_view kSaveFile = "LEGORac1";
constexpr unsigned kTrophyBits = 2;
constexpr unsigned kTrophyMask = 0b11;
constexpr std::array kSavedTrophies{settings::Trophy::Gold, settings::Trophy::Silver,
                                    settings::Trophy::Bronze};

template <typename Bits, typename Insert>
bool MergeBits(Bits bits, std::size_t count, Insert insert) {
    bool changed = false;
    for (std::size_t bit = 0; bit < count && bit < sizeof(Bits) * 8; ++bit) {
        if (((bits >> bit) & 1U) != 0) {
            changed = insert(bit) || changed;
        }
    }
    return changed;
}

bool InsertNumber(std::set<std::int32_t>& numbers, std::size_t number) {
    return numbers.insert(static_cast<std::int32_t>(number)).second;
}

bool MergeUnlocks(settings::Progress& progress, const assets::OriginalSave& save,
                  const assets::RaceCatalog& catalog) {
    const std::span<const assets::CircuitEntry> circuits = catalog.Circuits();
    bool changed = MergeBits(save.circuits, circuits.size(), [&](std::size_t circuit) {
        return circuit != 0 && progress.unlockedCircuits.insert(circuits[circuit].name).second;
    });
    changed = MergeBits(save.partSets, sizeof(save.partSets) * 8,
                        [&](std::size_t part) { return InsertNumber(progress.partSets, part); }) ||
              changed;
    return MergeBits(
               save.beatenRecords, settings::kRecordRaces,
               [&](std::size_t race) { return InsertNumber(progress.beatenRecords, race); }) ||
           changed;
}

bool MergeTime(std::optional<settings::BestTime>& held, const assets::SavedTime& saved) {
    if (saved.ms == 0 || (held && std::cmp_less_equal(held->ms, saved.ms))) {
        return false;
    }
    held = settings::BestTime{.ms = saved.ms, .holder = Utf8FromUtf16(saved.holder)};
    return true;
}

bool MergeTimes(settings::Progress& progress, const assets::OriginalSave& save,
                const assets::RaceCatalog& catalog) {
    bool changed = false;
    for (std::size_t race = 0; race < assets::kSavedRaces && race < catalog.Races().size();
         ++race) {
        const assets::RaceEntry& entry = catalog.Races()[race];
        const assets::SavedTime& lap = save.bestLaps.at(race);
        const assets::SavedTime& total = save.bestRaces.at(race);
        if (entry.isMirrored || (lap.ms == 0 && total.ms == 0)) {
            continue;
        }
        settings::TrackRecords& records = progress.bestTimes[assets::ToUpperAscii(entry.folder)];
        changed = MergeTime(records.lap, lap) || changed;
        changed = MergeTime(records.race, total) || changed;
    }
    return changed;
}

bool MergeTrophies(settings::Progress& progress, const assets::OriginalSave& save,
                   const assets::RaceCatalog& catalog) {
    bool changed = false;
    for (const assets::SavedRacer& racer : save.racers) {
        const unsigned trophies = racer.trophies;
        for (std::size_t circuit = 0; circuit < catalog.Circuits().size(); ++circuit) {
            const unsigned held = (trophies >> (circuit * kTrophyBits)) & kTrophyMask;
            if (held != 0) {
                changed = settings::AwardTrophy(progress, catalog.Circuits()[circuit].name,
                                                kSavedTrophies.at(held - 1)) ||
                          changed;
            }
        }
    }
    return changed;
}

Result<std::vector<std::byte>> ReadSaveFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return Fail(ErrorCode::Io, std::format("can't read {}", Utf8FromPath(path)));
    }
    std::vector<std::byte> bytes;
    std::ranges::transform(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(),
                           std::back_inserter(bytes),
                           [](char value) { return static_cast<std::byte>(value); });
    return bytes;
}

std::optional<int> FolderNumber(const std::filesystem::path& folder) {
    const std::string name = Utf8FromPath(folder.filename());
    int number = 0;
    const char* end = name.data() + name.size();
    const auto [last, error] = std::from_chars(name.data(), end, number);
    return error == std::errc{} && last == end ? std::optional(number) : std::nullopt;
}

}

bool MergeOriginalSave(settings::Progress& progress, const assets::OriginalSave& save,
                       const assets::RaceCatalog& catalog) {
    bool changed = MergeUnlocks(progress, save, catalog);
    changed = MergeTimes(progress, save, catalog) || changed;
    return MergeTrophies(progress, save, catalog) || changed;
}

Result<bool> ImportOriginalSave(const std::filesystem::path& saveFile,
                                const std::filesystem::path& progressFile, const assets::Vfs& vfs) {
    Result<assets::OriginalSave> save = ReadSaveFile(saveFile).and_then(
        [](const std::vector<std::byte>& bytes) { return assets::ParseOriginalSave(bytes); });
    Result<assets::RaceCatalog> catalog =
        save.and_then([&](const assets::OriginalSave&) { return assets::RaceCatalog::Load(vfs); });
    Result<settings::Progress> progress = catalog.and_then(
        [&](const assets::RaceCatalog&) { return settings::LoadProgress(progressFile); });
    if (!progress) {
        return std::unexpected(std::move(progress.error()));
    }
    if (!MergeOriginalSave(*progress, *save, *catalog)) {
        log::Info("{} adds nothing to {}", Utf8FromPath(saveFile), Utf8FromPath(progressFile));
        return false;
    }
    return settings::SaveProgress(*progress, progressFile).transform([&] {
        log::Info("imported {} into {}", Utf8FromPath(saveFile), Utf8FromPath(progressFile));
        return true;
    });
}

std::vector<std::filesystem::path> FindOriginalSaves(const std::filesystem::path& install) {
    std::vector<std::pair<int, std::filesystem::path>> numbered;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(install / kSaveFolder, error)) {
        const std::optional<int> number = FolderNumber(entry.path());
        std::filesystem::path file = entry.path() / kSaveFile;
        if (number && std::filesystem::is_regular_file(file, error)) {
            numbered.emplace_back(*number, std::move(file));
        }
    }
    std::ranges::sort(numbered);
    std::vector<std::filesystem::path> files;
    files.reserve(numbered.size());
    for (auto& [number, file] : numbered) {
        files.push_back(std::move(file));
    }
    return files;
}

}
