#pragma once

#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "assets/vfs_layer.hpp"
#include "core/result.hpp"

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

}
