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
#include "core/utf8_path.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kSaveFolder = "Save";
constexpr std::string_view kSaveFile = "LEGORac1";
constexpr unsigned kTrophyBits = 2;
constexpr unsigned kTrophyMask = 0b11;
constexpr std::array kSavedTrophies{settings::Trophy::Gold, settings::Trophy::Silver,
                                    settings::Trophy::Bronze};

constexpr std::size_t kTimeTrialRaces = 12;
constexpr char32_t kReplacement = 0xFFFD;
constexpr char16_t kHighSurrogate = 0xD800;
constexpr char16_t kLowSurrogate = 0xDC00;
constexpr char16_t kSurrogateEnd = 0xE000;
constexpr std::array<unsigned, 4> kUtf8Lead{0x00, 0xC0, 0xE0, 0xF0};

unsigned ContinuationBytes(char32_t point) {
    if (point < 0x800) {
        return 1;
    }
    return point < 0x10000 ? 2 : 3;
}

void AppendUtf8(std::string& utf8, char32_t point) {
    if (point < 0x80) {
        utf8.push_back(static_cast<char>(point));
        return;
    }
    const unsigned extra = ContinuationBytes(point);
    utf8.push_back(static_cast<char>(kUtf8Lead.at(extra) | (point >> (6U * extra))));
    for (unsigned byte = extra; byte > 0; --byte) {
        utf8.push_back(static_cast<char>(0x80U | ((point >> (6U * (byte - 1))) & 0x3FU)));
    }
}

bool IsLowSurrogate(char16_t unit) {
    return unit >= kLowSurrogate && unit < kSurrogateEnd;
}

std::string Utf8(std::u16string_view text) {
    std::string utf8;
    for (std::size_t at = 0; at < text.size(); ++at) {
        const char16_t unit = text[at];
        const bool isHigh = unit >= kHighSurrogate && unit < kLowSurrogate;
        if (isHigh && at + 1 < text.size() && IsLowSurrogate(text[at + 1])) {
            AppendUtf8(
                utf8, 0x10000U + ((unit - kHighSurrogate) << 10U) + (text[at + 1] - kLowSurrogate));
            ++at;
        } else {
            AppendUtf8(utf8,
                       isHigh || IsLowSurrogate(unit) ? kReplacement : static_cast<char32_t>(unit));
        }
    }
    return utf8;
}

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
               save.beatenRecords, kTimeTrialRaces,
               [&](std::size_t race) { return InsertNumber(progress.beatenRecords, race); }) ||
           changed;
}

bool MergeTime(std::optional<settings::BestTime>& held, const assets::SavedTime& saved) {
    if (saved.ms == 0 || (held && std::cmp_less_equal(held->ms, saved.ms))) {
        return false;
    }
    held = settings::BestTime{.ms = saved.ms, .holder = Utf8(saved.holder)};
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
