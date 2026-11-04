#include "settings/settings_file.hpp"

#include <format>
#include <string_view>

#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "settings/toml_file.hpp"

namespace rr::settings {
namespace {

void AssignAll(Registry& registry, const toml::table& root, std::string_view fileName) {
    for (const TomlSetting& setting : FlattenSettings(root)) {
        const Result<void> assigned =
            setting.value ? registry.Assign(setting.key, *setting.value)
                          : Fail(ErrorCode::InvalidData,
                                 std::format("{} has an unsupported type", setting.key));
        if (!assigned) {
            log::Warn("{}: {}, skipping it", fileName, assigned.error().message);
        }
    }
}

}

Result<void> LoadSettings(Registry& registry, const std::filesystem::path& path) {
    return ReadTomlFile(path).transform(
        [&](const TomlFile& file) { AssignAll(registry, file.table, Utf8FromPath(path)); });
}

Result<void> SaveSettings(const Registry& registry, const std::filesystem::path& path) {
    return WriteTomlFile({.table = NestSettings(registry.AllValues()), .leadingComments = {}},
                         path);
}

}
