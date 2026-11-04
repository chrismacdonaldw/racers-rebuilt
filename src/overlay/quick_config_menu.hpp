#pragma once

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "settings/quick_configs.hpp"
#include "settings/registry.hpp"

namespace rr::overlay {

class QuickConfigMenu {
public:
    explicit QuickConfigMenu(settings::QuickConfigs configs) : configs_(std::move(configs)) {}

    /// Returns the keys that applying a config changed.
    [[nodiscard]] std::vector<std::string> DrawMenu(settings::Registry& registry,
                                                    std::string_view menu);

    /// Call outside any menu.
    void DrawDialogs(const settings::Registry& registry);

    [[nodiscard]] const settings::QuickConfigs& Configs() const { return configs_; }

private:
    void DrawSave(const settings::Registry& registry);
    void DrawDelete(const settings::Registry& registry);

    settings::QuickConfigs configs_;
    std::array<char, 64> name_{};
    std::string pendingDelete_;
    std::string error_;
    bool opensSave_ = false;
    bool opensDelete_ = false;
};

}
