#include "assets/original_save.hpp"

#include <algorithm>
#include <format>
#include <numeric>
#include <utility>

namespace rr::assets {
namespace {

constexpr std::size_t kBlockSize = 128;
constexpr std::size_t kBlockData = kBlockSize - 1;
constexpr std::size_t kHeaderBlocks = 9;
constexpr std::size_t kRecordBlocks = 5;
constexpr std::size_t kRecordCountOffset = 0x002;
constexpr std::size_t kFirstRecordOffset = 0x003;
constexpr std::size_t kPartSetsOffset = 0x005;
constexpr std::size_t kCircuitsOffset = 0x006;
constexpr std::size_t kBeatenRecordsOffset = 0x007;
constexpr std::size_t kScoresOffset = 0x009;
constexpr std::size_t kScoreSize = 0x40;
constexpr std::size_t kRaceTimeOffset = 0x20;
constexpr std::size_t kNameUnits = 14;
constexpr std::size_t kLookOffset = 0x01C;
constexpr std::size_t kChassisOffset = 0x021;
constexpr std::size_t kChassisLength = 8;
constexpr std::size_t kCarOffset = 0x029;
constexpr std::size_t kTrophyOffset = 0x22B;
constexpr unsigned kExpressionMask = 0x7FU;
constexpr std::uint16_t kMagic = 0x524C;

unsigned ByteAt(std::span<const std::byte> data, std::size_t offset) {
    return std::to_integer<unsigned>(data[offset]);
}

std::uint16_t U16At(std::span<const std::byte> data, std::size_t offset) {
    return static_cast<std::uint16_t>(ByteAt(data, offset) | (ByteAt(data, offset + 1) << 8U));
}

std::uint32_t U32At(std::span<const std::byte> data, std::size_t offset) {
    return U16At(data, offset) | (static_cast<std::uint32_t>(U16At(data, offset + 2)) << 16U);
}

std::u16string NameAt(std::span<const std::byte> data, std::size_t offset) {
    std::u16string name;
    for (std::size_t unit = 0; unit < kNameUnits; ++unit) {
        const std::uint16_t letter = U16At(data, offset + (unit * 2));
        if (letter == 0) {
            break;
        }
        name.push_back(static_cast<char16_t>(letter));
    }
    return name;
}

SavedTime TimeAt(std::span<const std::byte> data, std::size_t offset) {
    return {.ms = U32At(data, offset), .holder = NameAt(data, offset + 4)};
}

std::uint8_t U8At(std::span<const std::byte> data, std::size_t offset) {
    return static_cast<std::uint8_t>(ByteAt(data, offset));
}

std::string ChassisAt(std::span<const std::byte> record) {
    std::string chassis;
    for (const std::byte letter : record.subspan(kChassisOffset, kChassisLength)) {
        if (letter == std::byte{0}) {
            break;
        }
        chassis.push_back(static_cast<char>(letter));
    }
    return chassis;
}

CarData CarAt(std::span<const std::byte> record) {
    CarData car{};
    std::ranges::copy(record.subspan(kCarOffset, kCarDataSize), car.begin());
    return car;
}

SavedRacer RacerAt(std::span<const std::byte> record) {
    return {
        .name = NameAt(record, 0),
        .look = {.face = U8At(record, kLookOffset),
                 .hat = U8At(record, kLookOffset + 1),
                 .legs = U8At(record, kLookOffset + 2),
                 .torso = U8At(record, kLookOffset + 3),
                 .expression =
                     static_cast<std::uint8_t>(ByteAt(record, kLookOffset + 4) & kExpressionMask)},
        .chassis = ChassisAt(record),
        .car = CarAt(record),
        .trophies = U16At(record, kTrophyOffset),
    };
}

Result<std::vector<std::byte>> BlockData(std::span<const std::byte> file, std::size_t first,
                                         std::size_t count) {
    if ((first + count) * kBlockSize > file.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("the save ends before block {}", first + count - 1));
    }
    std::vector<std::byte> data;
    data.reserve(count * kBlockData);
    for (std::size_t block = first; block < first + count; ++block) {
        const std::span<const std::byte> bytes = file.subspan(block * kBlockSize, kBlockSize);
        const unsigned sum = std::accumulate(bytes.begin(), bytes.end() - 1, 0U,
                                             [](unsigned total, std::byte value) {
                                                 return total + std::to_integer<unsigned>(value);
                                             });
        if ((sum & 0xFFU) != ByteAt(bytes, kBlockData)) {
            return Fail(ErrorCode::InvalidData,
                        std::format("save block {} fails its checksum", block));
        }
        data.insert(data.end(), bytes.begin(), bytes.end() - 1);
    }
    return data;
}

Result<std::vector<SavedRacer>> Racers(std::span<const std::byte> file, std::size_t count,
                                       std::size_t firstOffset) {
    if (firstOffset % kBlockSize != 0) {
        return Fail(ErrorCode::InvalidData,
                    std::format("the first save record at {} isn't on a block", firstOffset));
    }
    std::vector<SavedRacer> racers;
    racers.reserve(count);
    for (std::size_t record = 0; record < count; ++record) {
        const Result<std::vector<std::byte>> data =
            BlockData(file, (firstOffset / kBlockSize) + (record * kRecordBlocks), kRecordBlocks);
        if (!data) {
            return std::unexpected(data.error());
        }
        racers.push_back(RacerAt(*data));
    }
    return racers;
}

Result<std::vector<std::byte>> Header(std::span<const std::byte> file) {
    if (file.size() % kBlockSize != 0) {
        return Fail(
            ErrorCode::InvalidData,
            std::format("a save is whole {}-byte blocks, not {} bytes", kBlockSize, file.size()));
    }
    Result<std::vector<std::byte>> header = BlockData(file, 0, kHeaderBlocks);
    if (header && U16At(*header, 0) != kMagic) {
        return Fail(ErrorCode::InvalidData, "the save doesn't start with LR");
    }
    return header;
}

Result<std::vector<SavedRacer>> HeaderRacers(std::span<const std::byte> file,
                                             std::span<const std::byte> header) {
    return Racers(file, ByteAt(header, kRecordCountOffset), U16At(header, kFirstRecordOffset));
}

}

Result<OriginalSave> ParseOriginalSave(std::span<const std::byte> bytes) {
    const Result<std::vector<std::byte>> header = Header(bytes);
    if (!header) {
        return std::unexpected(header.error());
    }
    Result<std::vector<SavedRacer>> racers = HeaderRacers(bytes, *header);
    if (!racers) {
        return std::unexpected(std::move(racers.error()));
    }
    OriginalSave save{.partSets = static_cast<std::uint8_t>(ByteAt(*header, kPartSetsOffset)),
                      .circuits = static_cast<std::uint8_t>(ByteAt(*header, kCircuitsOffset)),
                      .beatenRecords = U16At(*header, kBeatenRecordsOffset),
                      .bestLaps = {},
                      .bestRaces = {},
                      .racers = std::move(*racers)};
    for (std::size_t race = 0; race < kSavedRaces; ++race) {
        const std::size_t score = kScoresOffset + (race * kScoreSize);
        save.bestLaps.at(race) = TimeAt(*header, score);
        save.bestRaces.at(race) = TimeAt(*header, score + kRaceTimeOffset);
    }
    return save;
}

Result<std::vector<SavedRacer>> ParseRacerFile(std::span<const std::byte> bytes) {
    return Header(bytes).and_then(
        [&](const std::vector<std::byte>& header) { return HeaderRacers(bytes, header); });
}

}
