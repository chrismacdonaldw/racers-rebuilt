#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace rr::sim {

/// What a physics-driven computer racer does beyond steering its route at its pace.
struct AiCraft {
    bool brakes = false;
    bool passes = false;
    bool unsticks = false;
    bool aims = false;
    bool racingLine = false;

    [[nodiscard]] static constexpr AiCraft All() {
        return {.brakes = true, .passes = true, .unsticks = true, .aims = true, .racingLine = true};
    }

    friend bool operator==(const AiCraft&, const AiCraft&) = default;
};

/// One letter per skill in a fixed order, such as `bpua` for all of them.
[[nodiscard]] std::string CraftLetters(const AiCraft& craft);

/// Empty, unknown or repeated letters fail.
[[nodiscard]] std::optional<AiCraft> ParseCraftLetters(std::string_view letters);

}
