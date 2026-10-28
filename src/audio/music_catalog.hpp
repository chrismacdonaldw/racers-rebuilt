#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::audio {

/// Track ids are the lowercase names of the install's `.tun` files, such as `circuit1`.
class MusicCatalog {
public:
    [[nodiscard]] static Result<MusicCatalog> Scan(const assets::Vfs& vfs);

    [[nodiscard]] const std::vector<std::string>& Ids() const { return ids_; }
    [[nodiscard]] bool Contains(std::string_view id) const;

private:
    explicit MusicCatalog(std::vector<std::string> ids) : ids_(std::move(ids)) {}

    std::vector<std::string> ids_;
};

}
