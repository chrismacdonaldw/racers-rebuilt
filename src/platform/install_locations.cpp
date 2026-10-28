#include "platform/install_locations.hpp"

namespace rr::platform {

namespace {

constexpr std::wstring_view kFileFlag = L" -f";

}

std::optional<std::filesystem::path> FolderFromUninstallCommand(std::wstring_view command) {
    const std::size_t flag = command.find(kFileFlag);
    if (flag == std::wstring_view::npos) {
        return std::nullopt;
    }
    std::wstring_view file = command.substr(flag + kFileFlag.size());
    if (file.starts_with(L'"')) {
        file.remove_prefix(1);
        file = file.substr(0, file.find(L'"'));
    } else {
        file = file.substr(0, file.find(L' '));
    }
    if (file.empty()) {
        return std::nullopt;
    }
    return std::filesystem::path(file).parent_path();
}

}
