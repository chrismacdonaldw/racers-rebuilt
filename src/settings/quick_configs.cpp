#include "settings/quick_configs.hpp"

#include <algorithm>
#include <format>
#include <system_error>
#include <utility>

#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "settings/quick_config.hpp"

namespace rr::settings {
namespace {

constexpr std::string_view kExtension = ".toml";

}

QuickConfigs::QuickConfigs(std::vector<SettingPreset> shipped, std::filesystem::path folder)
    : shipped_(std::move(shipped)), folder_(std::move(folder)) {}

void QuickConfigs::Reload(const Registry& registry) {
    user_.clear();
    std::error_code error;
    if (!std::filesystem::is_directory(folder_, error)) {
        return;
    }
    const std::vector<std::string> scope = QuickConfigScope(registry, shipped_);
    const std::string menu = shipped_.empty() ? std::string() : shipped_.front().menu;
    for (const auto& entry : std::filesystem::directory_iterator(folder_, error)) {
        const std::filesystem::path& path = entry.path();
        if (!entry.is_regular_file(error) || path.extension() != kExtension) {
            continue;
        }
        Result<QuickConfig> config = ReadQuickConfig(path);
        if (!config) {
            log::Warn("skipping quick config {}: {}", Utf8FromPath(path), config.error().message);
            continue;
        }
        user_.push_back({.slug = Utf8FromPath(path.stem()),
                         .preset = QuickConfigPreset(*config, registry, scope, menu)});
    }
    std::ranges::sort(user_, {}, [](const UserQuickConfig& config) { return config.preset.label; });
}

const UserQuickConfig* QuickConfigs::Find(std::string_view name) const {
    const std::string slug = QuickConfigSlug(name);
    const auto found = std::ranges::find_if(
        user_, [&](const UserQuickConfig& config) { return QuickConfigSlug(config.slug) == slug; });
    return found == user_.end() ? nullptr : &*found;
}

Result<void> QuickConfigs::Save(const Registry& registry, std::string_view name) {
    const std::vector<std::string> scope = QuickConfigScope(registry, shipped_);
    const UserQuickConfig* existing = Find(name);
    return WriteQuickConfig(SnapshotQuickConfig(registry, scope, std::string(name)),
                            PathOf(existing != nullptr ? existing->slug : QuickConfigSlug(name)))
        .transform([&] { Reload(registry); });
}

Result<void> QuickConfigs::Remove(const Registry& registry, std::string_view slug) {
    std::error_code error;
    std::filesystem::remove(PathOf(slug), error);
    if (error) {
        return Fail(ErrorCode::Io,
                    std::format("can't delete quick config {}: {}", slug, error.message()));
    }
    Reload(registry);
    return {};
}

std::filesystem::path QuickConfigs::PathOf(std::string_view slug) const {
    return folder_ / PathFromUtf8(std::string(slug).append(kExtension));
}

}
