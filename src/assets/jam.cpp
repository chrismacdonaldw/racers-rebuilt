#include "assets/jam.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "assets/byte_reader.hpp"
#include "assets/game_path.hpp"

namespace rr::assets {
namespace {

constexpr std::array kMagic{std::byte{'L'}, std::byte{'J'}, std::byte{'A'}, std::byte{'M'}};
constexpr std::uint32_t kRootOffset = 4;
constexpr std::size_t kNameBytes = 12;
constexpr std::size_t kFileRecordBytes = kNameBytes + 8;
constexpr std::size_t kDirectoryRecordBytes = kNameBytes + 4;

struct PendingDirectory {
    std::uint32_t offset = 0;
    std::string path;
};

Result<std::string> ReadName(ByteReader& reader) {
    Result<std::span<const std::byte>> raw = reader.ReadBytes(kNameBytes);
    if (!raw) {
        return std::unexpected(std::move(raw.error()));
    }
    const std::string_view name(reinterpret_cast<const char*>(raw->data()), raw->size());
    return NormalizeGameName(name.substr(0, name.find('\0')));
}

class JamWalker {
public:
    explicit JamWalker(std::span<const std::byte> bytes)
        : bytes_(bytes), entryBudget_(bytes.size() / kDirectoryRecordBytes) {}

    Result<JamIndex> Walk();

private:
    Result<void> ReadDirectory(const PendingDirectory& directory);
    Result<void> ReadFiles(ByteReader& reader, const std::string& path);
    Result<void> ReadSubdirectories(ByteReader& reader, const std::string& path);
    Result<std::uint32_t> ReadCount(ByteReader& reader, std::size_t recordBytes) const;
    Result<JamFileRange> ReadFileRange(ByteReader& reader, std::string_view name) const;

    std::span<const std::byte> bytes_;
    std::size_t entryBudget_;
    std::vector<IndexEntry> entries_;
    std::vector<JamFileRange> files_;
    std::vector<PendingDirectory> pending_;
    std::unordered_set<std::uint32_t> visited_;
};

Result<JamIndex> JamWalker::Walk() {
    if (!std::ranges::equal(bytes_.first(std::min(bytes_.size(), kMagic.size())), kMagic)) {
        return Fail(ErrorCode::InvalidData, "not a JAM archive: the LJAM signature is missing");
    }
    pending_.push_back({.offset = kRootOffset, .path = {}});
    visited_.insert(kRootOffset);
    while (!pending_.empty()) {
        const PendingDirectory directory = std::move(pending_.back());
        pending_.pop_back();
        Result<void> read = ReadDirectory(directory);
        if (!read) {
            return std::unexpected(std::move(read.error()));
        }
    }

    Result<PathIndex> paths = PathIndex::Build(std::move(entries_));
    if (!paths) {
        return Fail(ErrorCode::InvalidData, std::format("JAM entry {}", paths.error().message));
    }
    return JamIndex{.paths = std::move(*paths), .files = std::move(files_)};
}

Result<void> JamWalker::ReadDirectory(const PendingDirectory& directory) {
    ByteReader reader(bytes_, directory.offset);
    Result<void> read = ReadFiles(reader, directory.path).and_then([&] {
        return ReadSubdirectories(reader, directory.path);
    });
    if (!read) {
        return Fail(ErrorCode::InvalidData,
                    std::format("JAM directory \"/{}\": {}", directory.path, read.error().message));
    }
    return {};
}

Result<void> JamWalker::ReadFiles(ByteReader& reader, const std::string& path) {
    Result<std::uint32_t> count = ReadCount(reader, kFileRecordBytes);
    if (!count) {
        return std::unexpected(std::move(count.error()));
    }
    for (std::uint32_t i = 0; i < *count; ++i) {
        Result<std::string> name = ReadName(reader);
        if (!name) {
            return std::unexpected(std::move(name.error()));
        }
        Result<JamFileRange> range = ReadFileRange(reader, *name);
        if (!range) {
            return std::unexpected(std::move(range.error()));
        }
        const auto slot = static_cast<std::uint32_t>(files_.size());
        files_.push_back(*range);
        entries_.push_back(
            {.path = JoinGamePath(path, *name), .info = {.kind = EntryKind::File, .slot = slot}});
    }
    return {};
}

Result<void> JamWalker::ReadSubdirectories(ByteReader& reader, const std::string& path) {
    Result<std::uint32_t> count = ReadCount(reader, kDirectoryRecordBytes);
    if (!count) {
        return std::unexpected(std::move(count.error()));
    }
    for (std::uint32_t i = 0; i < *count; ++i) {
        Result<std::string> name = ReadName(reader);
        if (!name) {
            return std::unexpected(std::move(name.error()));
        }
        Result<std::uint32_t> offset = reader.ReadU32();
        if (!offset) {
            return std::unexpected(std::move(offset.error()));
        }
        if (!visited_.insert(*offset).second) {
            return Fail(ErrorCode::InvalidData,
                        std::format("{} points at a directory listed elsewhere", *name));
        }
        std::string childPath = JoinGamePath(path, *name);
        entries_.push_back({.path = childPath, .info = {.kind = EntryKind::Directory}});
        pending_.push_back({.offset = *offset, .path = std::move(childPath)});
    }
    return {};
}

Result<std::uint32_t> JamWalker::ReadCount(ByteReader& reader, std::size_t recordBytes) const {
    Result<std::uint32_t> count = reader.ReadU32();
    if (!count) {
        return count;
    }
    const bool fitsDirectory = std::size_t{*count} * recordBytes <= reader.Remaining();
    const bool fitsArchive = entries_.size() + *count <= entryBudget_;
    if (!fitsDirectory || !fitsArchive) {
        return Fail(ErrorCode::InvalidData,
                    std::format("lists {} entries, more than the archive can hold", *count));
    }
    return count;
}

Result<JamFileRange> JamWalker::ReadFileRange(ByteReader& reader, std::string_view name) const {
    Result<std::uint32_t> offset = reader.ReadU32();
    Result<std::uint32_t> sizeBytes =
        offset.and_then([&](std::uint32_t) { return reader.ReadU32(); });
    if (!sizeBytes) {
        return std::unexpected(std::move(sizeBytes.error()));
    }
    if (std::size_t{*offset} + *sizeBytes > bytes_.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} at offset {} with {} bytes runs past the end of the archive",
                                name, *offset, *sizeBytes));
    }
    return JamFileRange{.offset = *offset, .sizeBytes = *sizeBytes};
}

}

Result<JamIndex> ParseJam(std::span<const std::byte> bytes) {
    return JamWalker(bytes).Walk();
}

}
