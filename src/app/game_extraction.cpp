#include "app/game_extraction.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/install.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"

namespace rr::app {
namespace {

using DirectoryIterator = std::filesystem::directory_iterator;

constexpr std::array<std::string_view, 3> kExtractedExtensions{".JAM", ".TUN", ".AVI"};
constexpr std::string_view kPartialSuffix = ".partial";

struct Stamp {
    std::uint32_t version = 0;
    std::filesystem::path source;
};

std::unexpected<Error> IoFailure(std::string_view action, const std::filesystem::path& path,
                                 const std::error_code& error) {
    return Fail(ErrorCode::Io,
                std::format("can't {} {}: {}", action, Utf8FromPath(path), error.message()));
}

bool IsExtracted(const std::filesystem::directory_entry& entry) {
    std::error_code error;
    if (!entry.is_regular_file(error)) {
        return false;
    }
    const std::string extension = assets::ToUpperAscii(Utf8FromPath(entry.path().extension()));
    return std::ranges::contains(kExtractedExtensions, extension);
}

Result<void> CopyGameFiles(const std::filesystem::path& install,
                           const std::filesystem::path& target) {
    std::error_code error;
    std::filesystem::create_directories(target, error);
    if (error) {
        return IoFailure("create", target, error);
    }
    for (DirectoryIterator iterator(install, error); !error && iterator != DirectoryIterator();
         iterator.increment(error)) {
        if (!IsExtracted(*iterator)) {
            continue;
        }
        const std::filesystem::path& file = iterator->path();
        std::filesystem::copy_file(file, target / file.filename(),
                                   std::filesystem::copy_options::overwrite_existing, error);
        if (error) {
            return IoFailure("copy", file, error);
        }
    }
    if (error) {
        return IoFailure("list", install, error);
    }
    return {};
}

Result<void> WriteStamp(const std::filesystem::path& folder, const std::filesystem::path& source) {
    const std::filesystem::path path = folder / kExtractionStamp;
    std::ofstream file(path, std::ios::binary);
    file << kExtractionVersion << '\n' << Utf8FromPath(source) << '\n';
    if (!file) {
        return Fail(ErrorCode::Io, std::format("can't write {}", Utf8FromPath(path)));
    }
    return {};
}

std::optional<Stamp> ReadStamp(const std::filesystem::path& folder) {
    std::ifstream file(folder / kExtractionStamp, std::ios::binary);
    Stamp stamp;
    std::string source;
    if (!(file >> stamp.version) || !std::getline(file >> std::ws, source)) {
        return std::nullopt;
    }
    stamp.source = PathFromUtf8(source);
    return stamp;
}

Result<void> ReplaceFolder(const std::filesystem::path& from, const std::filesystem::path& to) {
    std::error_code error;
    std::filesystem::remove_all(to, error);
    if (error) {
        return IoFailure("remove", to, error);
    }
    std::filesystem::rename(from, to, error);
    if (error) {
        return IoFailure("rename", from, error);
    }
    return {};
}

bool IsSamePath(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code error;
    return a.lexically_normal() == b.lexically_normal() || std::filesystem::equivalent(a, b, error);
}

bool IsStampCurrent(const std::filesystem::path& folder,
                    const std::optional<std::filesystem::path>& gamePath) {
    const std::string name = Utf8FromPath(folder);
    const std::optional<Stamp> stamp = ReadStamp(folder);
    if (!stamp) {
        log::Info("no extracted game files in {}", name);
        return false;
    }
    if (stamp->version != kExtractionVersion) {
        log::Info("game files in {} were extracted by version {}, extracting again", name,
                  stamp->version);
        return false;
    }
    if (gamePath && !IsSamePath(*gamePath, stamp->source) && !IsSamePath(*gamePath, folder)) {
        log::Info("game.path is now {}, extracting again", Utf8FromPath(*gamePath));
        return false;
    }
    return true;
}

}

Result<void> ExtractGameFiles(const std::filesystem::path& install,
                              const std::filesystem::path& destination) {
    std::filesystem::path partial = destination;
    partial += kPartialSuffix;
    std::error_code error;
    std::filesystem::remove_all(partial, error);
    Result<void> extracted = CopyGameFiles(install, partial)
                                 .and_then([&] { return WriteStamp(partial, install); })
                                 .and_then([&] { return ReplaceFolder(partial, destination); });
    if (!extracted) {
        std::filesystem::remove_all(partial, error);
    }
    return extracted;
}

std::optional<GameInstall> OpenExtractedCopy(const std::filesystem::path& folder,
                                             const std::optional<std::filesystem::path>& gamePath) {
    if (!IsStampCurrent(folder, gamePath)) {
        return std::nullopt;
    }
    Result<assets::JamArchive> jam = assets::OpenInstall(folder);
    if (!jam) {
        log::Warn("extracted game files rejected, extracting again: {}", jam.error().message);
        return std::nullopt;
    }
    log::Info("game files found in {} (extracted copy)", Utf8FromPath(folder));
    return GameInstall{.folder = folder, .jam = std::move(*jam)};
}

}
