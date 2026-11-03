#pragma once

#include <cstddef>
#include <optional>

#include "assets/event_id.hpp"
#include "core/duration.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/model_switches.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

inline constexpr assets::EventId kSphinxTrigger = 12;
inline constexpr assets::EventId kSphinxBlowUpEvent = 16;
inline constexpr Milliseconds kSphinxBlowUp{600};

struct Sphinx {
    static constexpr bool kIgnoresEnd = true;

    std::optional<std::size_t> entity;
    std::size_t model = 0;
    Milliseconds blowUpLeft{0};
    bool isDestroyed = false;

    void Activate(HazardContext& context);

    /// Ends the hazard once the blow-up has played, leaving the sphinx destroyed and hidden.
    [[nodiscard]] bool Tick(HazardContext& context);

    /// Since the hit, until the sphinx is destroyed.
    [[nodiscard]] std::optional<Milliseconds> BlowingUp() const;
};

[[nodiscard]] Sphinx LoadSphinx(const TrackEntities& entities, ModelSwitches& models);

}
