#include "platform/install_locations.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <string>

namespace rr::platform {
namespace {

constexpr const wchar_t* kUninstallKey =
    LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\LEGO Racers)";
constexpr const wchar_t* kAppPathsKey =
    LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\LEGORacers.exe)";
constexpr const wchar_t* kVersionKey = LR"(SOFTWARE\LEGO Media\Games\LEGO Racers\0.90.000)";
constexpr const wchar_t* kInstallSubfolder = LR"(LEGO Media\Games\LEGO Racers)";
constexpr std::array<const wchar_t*, 3> kFixedInstallFolders{
    LR"(C:\Program Files (x86)\LEGO Media\Games\LEGO Racers)",
    LR"(C:\Program Files\LEGO Media\Games\LEGO Racers)",
    LR"(C:\Games\LEGO Racers)",
};

std::optional<std::wstring> ReadRegistryString(const wchar_t* key, const wchar_t* value) {
    constexpr DWORD kFlags = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_SUBKEY_WOW6432KEY;
    DWORD sizeBytes = 0;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key, value, kFlags, nullptr, nullptr, &sizeBytes) !=
        ERROR_SUCCESS) {
        return std::nullopt;
    }
    std::wstring text(sizeBytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key, value, kFlags, nullptr, text.data(), &sizeBytes) !=
        ERROR_SUCCESS) {
        return std::nullopt;
    }
    text.erase(std::ranges::find(text, L'\0'), text.end());
    return text;
}

std::optional<std::filesystem::path> EnvironmentFolder(const wchar_t* name) {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetEnvironmentVariableW(name, buffer.data(), MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::nullopt;
    }
    return std::filesystem::path(std::wstring_view(buffer.data(), length));
}

void AddUnique(std::vector<InstallCandidate>& candidates, std::filesystem::path folder,
               std::string_view source) {
    const bool isListed = std::ranges::any_of(
        candidates, [&](const InstallCandidate& candidate) { return candidate.folder == folder; });
    if (!isListed) {
        candidates.push_back({.folder = std::move(folder), .source = source});
    }
}

}

std::vector<InstallCandidate> RegistryInstallCandidates() {
    std::vector<InstallCandidate> candidates;
    if (const std::optional<std::wstring> command =
            ReadRegistryString(kUninstallKey, L"UninstallString")) {
        if (std::optional<std::filesystem::path> folder = FolderFromUninstallCommand(*command)) {
            AddUnique(candidates, std::move(*folder), "uninstall registry entry");
        }
    }
    if (const std::optional<std::wstring> folder = ReadRegistryString(kAppPathsKey, L"Path")) {
        AddUnique(candidates, *folder, "App Paths registry entry");
    }
    return candidates;
}

std::vector<InstallCandidate> KnownInstallCandidates() {
    std::vector<InstallCandidate> candidates;
    for (const wchar_t* variable : {L"ProgramFiles(x86)", L"ProgramFiles"}) {
        if (const std::optional<std::filesystem::path> root = EnvironmentFolder(variable)) {
            AddUnique(candidates, *root / kInstallSubfolder, "default install folder");
        }
    }
    for (const wchar_t* folder : kFixedInstallFolders) {
        AddUnique(candidates, folder, "common install folder");
    }
    return candidates;
}

std::optional<std::uint32_t> InstalledLanguageId() {
    DWORD languageId = 0;
    DWORD sizeBytes = sizeof(languageId);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, kVersionKey, L"LangID",
                     RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6432KEY, nullptr, &languageId,
                     &sizeBytes) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return std::uint32_t{languageId};
}

}
