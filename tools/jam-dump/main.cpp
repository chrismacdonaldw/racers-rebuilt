#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <span>
#include <system_error>
#include <utility>

#include "assets/jam_archive.hpp"
#include "core/utf8_path.hpp"

namespace rr::tools {
namespace {

constexpr int kExitUsage = 2;

void PrintEntries(const assets::JamArchive& archive) {
    for (const assets::IndexEntry& entry : archive.Index().Entries()) {
        if (entry.info.kind == assets::EntryKind::Directory) {
            std::println("{}/", entry.path);
            continue;
        }
        Result<assets::FileData> data = archive.Read(entry.info);
        if (data) {
            std::println("{}  {}", entry.path, data->Bytes().size());
        } else {
            std::println(stderr, "{}: {}", entry.path, data.error().message);
        }
    }
}

Result<void> WriteEntry(const assets::JamArchive& archive, const assets::IndexEntry& entry,
                        const std::filesystem::path& outputFolder) {
    const std::filesystem::path target = outputFolder / PathFromUtf8(entry.path);
    std::error_code error;
    const bool isDirectory = entry.info.kind == assets::EntryKind::Directory;
    std::filesystem::create_directories(isDirectory ? target : target.parent_path(), error);
    if (error) {
        return Fail(ErrorCode::Io, std::format("can't create the folder for {}: {}",
                                               Utf8FromPath(target), error.message()));
    }
    if (isDirectory) {
        return {};
    }
    Result<assets::FileData> data = archive.Read(entry.info);
    if (!data) {
        return std::unexpected(std::move(data.error()));
    }
    const std::span<const std::byte> bytes = data->Bytes();
    std::ofstream file(target, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!file) {
        return Fail(ErrorCode::Io, std::format("can't write {}", Utf8FromPath(target)));
    }
    return {};
}

Result<void> Extract(const assets::JamArchive& archive, const std::filesystem::path& outputFolder) {
    for (const assets::IndexEntry& entry : archive.Index().Entries()) {
        Result<void> written = WriteEntry(archive, entry, outputFolder);
        if (!written) {
            return written;
        }
    }
    std::println("extracted {} entries to {}", archive.Index().Entries().size(),
                 Utf8FromPath(outputFolder));
    return {};
}

int Run(std::span<char* const> args) {
    if (args.empty() || args.size() > 2) {
        std::println(stderr, "usage: jam-dump <LEGO.JAM> [output folder]");
        std::println(stderr,
                     "Lists every entry with its size, or extracts them all to the folder.");
        return kExitUsage;
    }
    Result<assets::JamArchive> archive = assets::JamArchive::Open(std::filesystem::path(args[0]));
    if (!archive) {
        std::println(stderr, "{}", archive.error().message);
        return 1;
    }
    if (args.size() == 1) {
        PrintEntries(*archive);
        return 0;
    }
    Result<void> extracted = Extract(*archive, std::filesystem::path(args[1]));
    if (!extracted) {
        std::println(stderr, "{}", extracted.error().message);
        return 1;
    }
    return 0;
}

}
}

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv) {
    return rr::tools::Run(std::span(argv, static_cast<std::size_t>(argc)).subspan(1));
}
