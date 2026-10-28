#include "assets/jam_archive.hpp"

#include <format>
#include <utility>

#include "core/assert.hpp"
#include "core/utf8_path.hpp"

namespace rr::assets {

Result<JamArchive> JamArchive::Open(const std::filesystem::path& path) {
    Result<platform::MappedFile> file = platform::MappedFile::Open(path);
    if (!file) {
        return std::unexpected(std::move(file.error()));
    }
    Result<JamIndex> index = ParseJam(file->Bytes());
    if (!index) {
        return Fail(index.error().code,
                    std::format("{}: {}", Utf8FromPath(path), index.error().message));
    }
    return JamArchive(std::move(*file), std::move(*index));
}

Result<FileData> JamArchive::Read(EntryInfo file) const {
    RR_ASSERT(file.kind == EntryKind::File && file.slot < index_.files.size());
    const JamFileRange range = index_.files[file.slot];
    return FileData(file_.Bytes().subspan(range.offset, range.sizeBytes));
}

JamArchive::JamArchive(platform::MappedFile file, JamIndex index)
    : file_(std::move(file)), index_(std::move(index)) {}

}
