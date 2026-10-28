#pragma once

#include <string_view>

#include "mods/hook_registry.hpp"
#include "settings/registry.hpp"

namespace rr::mods {

class Enhancement {
public:
    virtual ~Enhancement() = default;

    [[nodiscard]] virtual std::string_view Name() const = 0;

    /// Replays and time trials flag or refuse an enhancement that changes gameplay.
    [[nodiscard]] virtual bool ChangesGameplay() const = 0;

    /// Runs before settings load.
    virtual void DeclareSettings(settings::Registry& registry) const = 0;

    /// Runs once settings are loaded. `settings` must outlive the enhancement, and the
    /// enhancement must outlive `hooks`.
    virtual void Attach(HookRegistry& hooks, const settings::Registry& settings) = 0;
};

}
