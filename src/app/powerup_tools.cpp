#include "app/powerup_tools.hpp"

#include <format>
#include <string_view>

#include "sim/powerup/powerup_kind.hpp"

namespace rr::app {
namespace {

std::string_view KindName(sim::PowerupKind kind) {
    switch (kind) {
        case sim::PowerupKind::Cannonball:
            return "Cannonball";
        case sim::PowerupKind::GrapplingHook:
            return "Grappling hook";
        case sim::PowerupKind::Lightning:
            return "Lightning wand";
        case sim::PowerupKind::Missile:
            return "Homing missiles";
        case sim::PowerupKind::OilSlick:
            return "Oil slick";
        case sim::PowerupKind::Dynamite:
            return "Dynamite";
        case sim::PowerupKind::Magnet:
            return "Magnet";
        case sim::PowerupKind::Curse:
            return "Curse";
        case sim::PowerupKind::Shield:
            return "Shield";
        case sim::PowerupKind::Turbo:
            return "Turbo";
        case sim::PowerupKind::Warp:
            return "Warp";
        case sim::PowerupKind::Count:
            break;
    }
    return {};
}

std::string_view ColourName(assets::BrickColour colour) {
    switch (colour) {
        case assets::BrickColour::Red:
            return "red";
        case assets::BrickColour::Blue:
            return "blue";
        case assets::BrickColour::Green:
            return "green";
        case assets::BrickColour::Yellow:
            return "yellow";
    }
    return {};
}

}

std::string PowerupLabel(assets::BrickColour colour, std::uint8_t level) {
    return std::format("{} ({} {})", KindName(sim::KindFor(colour, level)), ColourName(colour),
                       level);
}

}
