#include "assets/install.hpp"

#include <array>
#include <format>
#include <system_error>
#include <utility>

#include "core/utf8_path.hpp"

namespace rr::assets {
namespace {

struct LanguageFolder {
    std::uint32_t langId;
    std::string_view folder;
};

constexpr std::array kLanguageFolders{
    LanguageFolder{.langId = 0x09, .folder = "ENGLISH"},
    LanguageFolder{.langId = 0x07, .folder = "GERMAN"},
    LanguageFolder{.langId = 0x040c, .folder = "FRENCH"},
    LanguageFolder{.langId = 0x0a, .folder = "SPANISH"},
    LanguageFolder{.langId = 0x10, .folder = "ITALIAN"},
    LanguageFolder{.langId = 0x13, .folder = "DUTCH"},
    LanguageFolder{.langId = 0x06, .folder = "DANISH"},
    LanguageFolder{.langId = 0x14, .folder = "NORWEGI"},
    LanguageFolder{.langId = 0x1d, .folder = "SWEDISH"},
};

constexpr std::array<std::string_view, 2> kRequiredDirectories{"GAMEDATA", "MENUDATA"};

}

Result<JamArchive> OpenInstall(const std::filesystem::path& folder) {
    const std::string folderName = Utf8FromPath(folder);
    std::error_code error;
    if (!std::filesystem::is_directory(folder, error)) {
        return Fail(ErrorCode::NotFound, std::format("{} doesn't exist", folderName));
    }
    const std::filesystem::path jamPath = folder / kJamFileName;
    if (!std::filesystem::is_regular_file(jamPath, error)) {
        return Fail(ErrorCode::NotFound, std::format("{} has no {}", folderName, kJamFileName));
    }
    Result<JamArchive> archive = JamArchive::Open(jamPath);
    if (!archive) {
        return archive;
    }
    for (const std::string_view directory : kRequiredDirectories) {
        const std::optional<EntryInfo> entry = archive->Index().Find(directory);
        if (!entry || entry->kind != EntryKind::Directory) {
            return Fail(ErrorCode::InvalidData, std::format("{} in {} has no {} folder",
                                                            kJamFileName, folderName, directory));
        }
    }
    return archive;
}

std::optional<std::string_view> LanguageForLangId(std::uint32_t langId) {
    for (const LanguageFolder& language : kLanguageFolders) {
        if (language.langId == langId) {
            return language.folder;
        }
    }
    return std::nullopt;
}

}
