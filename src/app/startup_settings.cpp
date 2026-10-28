#include "app/startup_settings.hpp"

#include <utility>

#include "app/audio_settings.hpp"
#include "app/game_settings.hpp"
#include "app/video_settings.hpp"
#include "core/assert.hpp"
#include "core/log.hpp"
#include "settings/settings_file.hpp"

namespace rr::app {
namespace {

settings::Registry DeclaredRegistry() {
    settings::Registry registry;
    DeclareGameSettings(registry);
    DeclareAudioSettings(registry);
    DeclareVideoSettings(registry);
    return registry;
}

}

Result<settings::Registry> LoadStartupSettings(
    const std::filesystem::path& file, std::span<const settings::SettingOverride> overrides) {
    settings::Registry registry = DeclaredRegistry();
    Result<void> loaded = settings::LoadSettings(registry, file);
    if (!loaded) {
        log::Warn("{}, using default settings", loaded.error().message);
        registry = DeclaredRegistry();
    }
    Result<void> applied = settings::ApplyOverrides(registry, overrides);
    if (!applied) {
        return std::unexpected(std::move(applied.error()));
    }
    return registry;
}

void PersistSettings(const std::filesystem::path& file, const settings::Registry& current,
                     std::span<const std::string_view> keys) {
    settings::Registry stored = DeclaredRegistry();
    Result<void> loaded = settings::LoadSettings(stored, file);
    if (!loaded) {
        log::Warn("{}, not saving settings over it", loaded.error().message);
        return;
    }
    for (const std::string_view key : keys) {
        const auto value = current.AllValues().find(key);
        if (value != current.AllValues().end()) {
            const Result<void> assigned = stored.Assign(key, value->second);
            RR_VERIFY(assigned.has_value());
        }
    }
    Result<void> saved = settings::SaveSettings(stored, file);
    if (!saved) {
        log::Warn("{}, settings weren't saved", saved.error().message);
    }
}

}
