#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>

#include "core/result.hpp"

namespace rr::platform {

/// The bytes keep their address across moves.
class MappedFile {
public:
    [[nodiscard]] static Result<MappedFile> Open(const std::filesystem::path& path);

    [[nodiscard]] std::span<const std::byte> Bytes() const;

private:
    struct Unmapper {
        void operator()(const void* view) const;
    };

    MappedFile(const void* view, std::size_t sizeBytes);

    std::unique_ptr<const void, Unmapper> view_;
    std::size_t sizeBytes_ = 0;
};

}
