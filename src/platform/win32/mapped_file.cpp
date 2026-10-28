#include "platform/mapped_file.hpp"

#include <format>
#include <string_view>
#include <system_error>

#include "core/utf8_path.hpp"

#include <windows.h>

namespace rr::platform {
namespace {

struct HandleCloser {
    void operator()(HANDLE handle) const { CloseHandle(handle); }
};

using UniqueHandle = std::unique_ptr<void, HandleCloser>;

std::unexpected<Error> FailLastError(std::string_view action, const std::filesystem::path& path) {
    const DWORD error = GetLastError();
    const bool isMissing = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    return Fail(isMissing ? ErrorCode::NotFound : ErrorCode::Io,
                std::format("can't {} {}: {}", action, Utf8FromPath(path),
                            std::system_category().message(static_cast<int>(error))));
}

}

Result<MappedFile> MappedFile::Open(const std::filesystem::path& path) {
    HANDLE rawFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (rawFile == INVALID_HANDLE_VALUE) {
        return FailLastError("open", path);
    }
    const UniqueHandle file(rawFile);

    LARGE_INTEGER size{};
    if (GetFileSizeEx(file.get(), &size) == 0) {
        return FailLastError("read the size of", path);
    }
    if (size.QuadPart == 0) {
        return MappedFile(nullptr, 0);
    }

    const UniqueHandle mapping(
        CreateFileMappingW(file.get(), nullptr, PAGE_READONLY, 0, 0, nullptr));
    if (mapping == nullptr) {
        return FailLastError("map", path);
    }
    const void* view = MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, 0);
    if (view == nullptr) {
        return FailLastError("map", path);
    }
    return MappedFile(view, static_cast<std::size_t>(size.QuadPart));
}

std::span<const std::byte> MappedFile::Bytes() const {
    if (view_ == nullptr) {
        return {};
    }
    return {static_cast<const std::byte*>(view_.get()), sizeBytes_};
}

MappedFile::MappedFile(const void* view, std::size_t sizeBytes)
    : view_(view), sizeBytes_(sizeBytes) {}

void MappedFile::Unmapper::operator()(const void* view) const {
    UnmapViewOfFile(view);
}

}
