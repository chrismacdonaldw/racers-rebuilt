#pragma once

#include <optional>
#include <string>
#include <vector>

#include "mods/mod_list.hpp"
#include "mods/mod_toggles.hpp"
#include "settings/registry.hpp"

namespace rr::overlay {

class ModMenu {
public:
    /// One row per mod, with why it's held back. Returns the keys a toggle changed.
    [[nodiscard]] std::vector<std::string> DrawRows(settings::Registry& registry,
                                                    const mods::ModList& mods);

    /// Call outside any menu. Returns the keys the answer changed.
    [[nodiscard]] std::vector<std::string> DrawQuestion(settings::Registry& registry,
                                                        const mods::ModList& mods);

private:
    std::optional<mods::ModToggle> pending_;
    bool opensQuestion_ = false;
};

}
