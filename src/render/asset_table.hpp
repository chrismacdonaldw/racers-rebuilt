#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "core/assert.hpp"

namespace rr::render {

/// Ids are indices. `Release` drops everything added after the last `Pin`, so pinned ids stay
/// valid.
template <typename T, typename Id>
class AssetTable {
public:
    Id Add(T asset) {
        assets_.push_back(std::move(asset));
        return Id{static_cast<std::uint32_t>(assets_.size() - 1)};
    }

    [[nodiscard]] bool Contains(Id id) const {
        return static_cast<std::size_t>(id) < assets_.size();
    }

    [[nodiscard]] const T& Get(Id id) const {
        RR_VERIFY(Contains(id));
        return assets_[static_cast<std::size_t>(id)];
    }

    void Pin() { pinned_ = assets_.size(); }

    void Release() {
        assets_.erase(assets_.begin() + static_cast<std::ptrdiff_t>(pinned_), assets_.end());
    }

private:
    std::vector<T> assets_;
    std::size_t pinned_ = 0;
};

}
