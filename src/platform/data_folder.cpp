#include "platform/data_folder.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <format>
#include <memory>
#include <system_error>

#include "core/utf8_path.hpp"

namespace rr::platform {
namespace {

constexpr const char* kAppFolderName = "racers-rebuilt";

struct SdlFree {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    void operator()(char* text) const { SDL_free(text); }
};

Result<std::filesystem::path> UserDataFolder() {
    const std::unique_ptr<char, SdlFree> prefPath(SDL_GetPrefPath(nullptr, kAppFolderName));
    if (prefPath == nullptr) {
        return Fail(ErrorCode::Platform,
                    std::format("can't create the data folder in %APPDATA%: {}", SDL_GetError()));
    }
    return PathFromUtf8(prefPath.get());
}

}

Result<std::filesystem::path> ExeFolder() {
    const char* basePath = SDL_GetBasePath();
    if (basePath == nullptr) {
        return Fail(ErrorCode::Platform,
                    std::format("can't find the exe folder: {}", SDL_GetError()));
    }
    return PathFromUtf8(basePath);
}

Result<std::filesystem::path> ResolveDataFolder() {
    Result<std::filesystem::path> exeFolder = ExeFolder();
    if (!exeFolder) {
        return exeFolder;
    }
    if (IsPortable(*exeFolder)) {
        return exeFolder;
    }
    return UserDataFolder();
}

bool IsPortable(const std::filesystem::path& exeFolder) {
    std::error_code error;
    return std::filesystem::is_regular_file(exeFolder / kPortableMarker, error);
}

}
