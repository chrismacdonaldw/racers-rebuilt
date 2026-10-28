#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace rr::assets {

/// Keyed by normalized VFS path. Main thread only.
template <typename T>
class AssetCache {
public:
    using Handle = std::shared_ptr<const T>;

    [[nodiscard]] Handle Find(std::string_view path) const {
        const auto entry = entries_.find(path);
        return entry == entries_.end() ? nullptr : entry->second;
    }

    Handle Insert(std::string path, T asset) {
        Handle handle = std::make_shared<const T>(std::move(asset));
        entries_.insert_or_assign(std::move(path), handle);
        return handle;
    }

    /// Drops every asset that no handle outside the cache still holds.
    void Prune() {
        std::erase_if(entries_, [](const auto& entry) { return entry.second.use_count() == 1; });
    }

    [[nodiscard]] std::size_t Size() const { return entries_.size(); }

private:
    std::map<std::string, Handle, std::less<>> entries_;
};

}
