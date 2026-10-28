#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace rr::platform {

struct InstallCandidate {
    std::filesystem::path folder;
    std::string_view source;
};

/// Uninstalls and moves leave stale entries.
[[nodiscard]] std::vector<InstallCandidate> RegistryInstallCandidates();

[[nodiscard]] std::vector<InstallCandidate> KnownInstallCandidates();

[[nodiscard]] std::optional<std::uint32_t> InstalledLanguageId();

/// Parses the `-f"<folder>\Uninst.isu"` argument of an InstallShield uninstall command.
[[nodiscard]] std::optional<std::filesystem::path> FolderFromUninstallCommand(
    std::wstring_view command);

}
