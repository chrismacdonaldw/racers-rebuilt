#pragma once

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/jam_archive.hpp"
#include "assets/vfs.hpp"
#include "assets/vfs_layer.hpp"
#include "core/result.hpp"
#include "game_files.hpp"

namespace rr::test {

struct Survey {
    std::size_t fileCount = 0;
    std::vector<std::string> failures;
};

[[nodiscard]] inline std::string_view Extension(std::string_view name) {
    const std::size_t dot = name.rfind('.');
    return dot == std::string_view::npos ? std::string_view{} : name.substr(dot + 1);
}

template <typename IsWanted, typename Check>
[[nodiscard]] Survey SurveyLayer(const assets::VfsLayer& layer, IsWanted isWanted, Check check) {
    Survey survey;
    for (const assets::IndexEntry& entry : layer.Index().Entries()) {
        if (entry.info.kind != assets::EntryKind::File || !isWanted(entry.Name())) {
            continue;
        }
        ++survey.fileCount;
        const Result<void> checked =
            layer.Read(entry.info).and_then([&](const assets::FileData& data) {
                return check(data.Bytes());
            });
        if (!checked) {
            survey.failures.push_back(std::format("{}: {}", entry.path, checked.error().message));
        }
    }
    return survey;
}

inline void CheckSurvey(const Survey& survey, std::size_t expectedFiles) {
    CHECK(survey.fileCount == expectedFiles);
    for (const std::string& failure : survey.failures) {
        FAIL_CHECK(failure);
    }
}

[[nodiscard]] inline Result<assets::FileData> ReadRetailFile(const assets::VfsLayer& layer,
                                                             std::string_view path) {
    const std::optional<assets::EntryInfo> entry = layer.Index().Find(path);
    if (!entry) {
        return Fail(ErrorCode::NotFound, std::format("{} isn't in the archive", path));
    }
    return layer.Read(*entry);
}

[[nodiscard]] inline std::vector<assets::IndexEntry> FilesWithExtension(
    const assets::VfsLayer& layer, std::string_view extension) {
    return layer.Index().Entries() | std::views::filter([&](const assets::IndexEntry& entry) {
               return entry.info.kind == assets::EntryKind::File &&
                      Extension(entry.Name()) == extension;
           }) |
           std::ranges::to<std::vector>();
}

template <typename Parse>
[[nodiscard]] auto ParseEntry(const assets::VfsLayer& layer, const assets::IndexEntry& entry,
                              Parse parse) {
    return layer.Read(entry.info).and_then([&](const assets::FileData& data) {
        return parse(data.Bytes());
    });
}

[[nodiscard]] inline std::optional<assets::JamArchive> OpenRetailJam() {
    const std::optional<std::filesystem::path> gameFolder = GameFolder();
    if (!gameFolder) {
        MESSAGE("RR_GAME_PATH isn't set, skipping");
        return std::nullopt;
    }
    Result<assets::JamArchive> archive = assets::JamArchive::Open(*gameFolder / "LEGO.JAM");
    REQUIRE(archive.has_value());
    return std::move(*archive);
}

[[nodiscard]] inline std::optional<assets::Vfs> OpenRetailVfs() {
    std::optional<assets::JamArchive> archive = OpenRetailJam();
    if (!archive) {
        return std::nullopt;
    }
    assets::Vfs vfs;
    vfs.Mount(std::make_unique<assets::JamArchive>(std::move(*archive)));
    return vfs;
}

}
