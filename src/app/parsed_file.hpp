#pragma once

#include <cstddef>
#include <format>
#include <span>
#include <string>

#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

template <typename Asset>
using Parser = Result<Asset> (*)(std::span<const std::byte>);

/// Errors name `path`.
template <typename Asset>
[[nodiscard]] Result<Asset> ReadParsed(const assets::Vfs& vfs, const std::string& path,
                                       Parser<Asset> parse) {
    return vfs.Read(path)
        .and_then([&](const assets::FileData& data) { return parse(data.Bytes()); })
        .transform_error([&](Error error) {
            error.message = std::format("{}: {}", path, error.message);
            return error;
        });
}

}
