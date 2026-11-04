#include "mods/mod_list.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <system_error>
#include <utility>

#include "assets/folder_layer.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"

namespace rr::mods {
namespace {

constexpr std::string_view kManifestName = "mod.toml";

Result<Mod> LoadMod(const std::filesystem::path& folder) {
    const std::filesystem::path manifestPath = folder / kManifestName;
    std::ifstream file(manifestPath, std::ios::binary);
    if (!file) {
        return Fail(ErrorCode::NotFound,
                    std::format("{} has no {}", Utf8FromPath(folder), kManifestName));
    }
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    Result<ModManifest> manifest = ParseModManifest(text, Utf8FromPath(manifestPath));
    if (!manifest) {
        return std::unexpected(std::move(manifest.error()));
    }
    const std::string folderName = Utf8FromPath(folder.filename());
    if (manifest->id != folderName) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}: id {} doesn't match its folder {}", Utf8FromPath(manifestPath),
                                manifest->id, folderName));
    }
    return Mod(std::move(*manifest), folder);
}

bool IsHigherPriority(const Mod& a, const Mod& b) {
    if (a.Manifest().priority != b.Manifest().priority) {
        return a.Manifest().priority > b.Manifest().priority;
    }
    return a.Manifest().id < b.Manifest().id;
}

}

ModList ModList::Discover(const std::filesystem::path& modsFolder) {
    std::vector<Mod> mods;
    std::vector<std::string> skipped;
    using DirectoryIterator = std::filesystem::directory_iterator;
    std::error_code error;
    for (DirectoryIterator entry(modsFolder, error); !error && entry != DirectoryIterator();
         entry.increment(error)) {
        std::error_code kindError;
        if (!entry->is_directory(kindError)) {
            continue;
        }
        Result<Mod> mod = LoadMod(entry->path());
        if (mod) {
            mods.push_back(std::move(*mod));
        } else {
            log::Warn("{}, skipping that mod", mod.error().message);
            skipped.push_back(std::move(mod.error().message));
        }
    }
    std::ranges::sort(mods, IsHigherPriority);
    return {std::move(mods), std::move(skipped)};
}

std::vector<bool> ModList::Toggles(const settings::Registry& registry) const {
    std::vector<bool> toggles;
    toggles.reserve(mods_.size());
    for (const Mod& mod : mods_) {
        toggles.push_back(registry.Get(mod.EnabledSetting()));
    }
    return toggles;
}

ModResolution ModList::Resolve(const settings::Registry& registry) const {
    return ResolveMods(mods_, Toggles(registry));
}

std::vector<const Mod*> ModList::Enabled(const settings::Registry& registry) const {
    std::vector<const Mod*> enabled;
    for (const std::size_t index : std::views::reverse(Resolve(registry).mountOrder)) {
        enabled.push_back(&mods_[index]);
    }
    return enabled;
}

std::vector<std::string> ModList::Apply(settings::Registry& registry,
                                        std::span<const ModSwitch> switches) const {
    std::vector<std::string> changed;
    for (const ModSwitch& change : switches) {
        const settings::SettingSpec<bool> setting = mods_[change.index].EnabledSetting();
        if (registry.Get(setting) != change.isOn) {
            registry.Set(setting, change.isOn);
            changed.emplace_back(setting.key);
        }
    }
    return changed;
}

std::vector<std::string> ModList::TurnOnRequired(settings::Registry& registry) const {
    return Apply(registry, RequiredSwitches(mods_, Toggles(registry)));
}

void ModList::DeclareSettings(settings::Registry& registry) const {
    for (const Mod& mod : mods_) {
        registry.Declare(mod.EnabledSetting());
    }
}

void ModList::MountEnabled(assets::Vfs& vfs, const settings::Registry& registry) const {
    const ModResolution resolution = Resolve(registry);
    for (const HeldMod& held : resolution.held) {
        log::Warn("mod {}: {}, so it isn't loaded", mods_[held.index].Manifest().id, held.reason);
    }
    for (const std::size_t index : resolution.mountOrder) {
        const Mod* mod = &mods_[index];
        std::error_code error;
        if (!std::filesystem::is_directory(mod->FilesFolder(), error)) {
            continue;
        }
        Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(mod->FilesFolder());
        if (!layer) {
            log::Warn("mod {}: {}, skipping its files", mod->Manifest().id, layer.error().message);
            continue;
        }
        vfs.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
        log::Info("mod {} {}: files mounted", mod->Manifest().id, mod->Manifest().version);
    }
}

}
