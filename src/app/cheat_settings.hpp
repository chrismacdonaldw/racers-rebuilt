#pragma once

#include <array>
#include <string_view>

#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"
#include "sim/race/cheats.hpp"

namespace rr::app {

struct CheatToggle {
    sim::Cheat cheat;
    settings::SettingSpec<bool> setting;
};

inline constexpr std::string_view kCheatsCategory = "Enhancements/Cheats";

inline constexpr std::array kCheatToggles{
    CheatToggle{.cheat = sim::Cheat::IgnoreSurfaces,
                .setting = {.key = "cheats.nslwj",
                            .label = "Ignore track surfaces (NSLWJ)",
                            .category = kCheatsCategory,
                            .changesGameplay = true}},
    CheatToggle{.cheat = sim::Cheat::HoverTurbo,
                .setting = {.key = "cheats.flyskyhgh",
                            .label = "Hover turbo (FLYSKYHGH)",
                            .category = kCheatsCategory,
                            .changesGameplay = true}},
    CheatToggle{.cheat = sim::Cheat::RedBricks,
                .setting = {.key = "cheats.pgllrd",
                            .label = "Red power-up bricks (PGLLRD)",
                            .category = kCheatsCategory,
                            .changesGameplay = true}},
    CheatToggle{.cheat = sim::Cheat::YellowBricks,
                .setting = {.key = "cheats.pgllyll",
                            .label = "Yellow power-up bricks (PGLLYLL)",
                            .category = kCheatsCategory,
                            .changesGameplay = true}},
    CheatToggle{.cheat = sim::Cheat::GreenBricks,
                .setting = {.key = "cheats.pgllgrn",
                            .label = "Green power-up bricks (PGLLGRN)",
                            .category = kCheatsCategory,
                            .changesGameplay = true}},
    CheatToggle{.cheat = sim::Cheat::MirroredBoss,
                .setting = {.key = "cheats.lnfrrrm",
                            .label = "Mirrored Rocket Racer tracks (LNFRRRM)",
                            .category = kCheatsCategory,
                            .changesGameplay = true}},
    CheatToggle{.cheat = sim::Cheat::RedOnly,
                .setting = {.key = "cheats.rpcrnly",
                            .label = "Only level 1 red power-ups (RPCRNLY)",
                            .category = kCheatsCategory,
                            .changesGameplay = true}},
    CheatToggle{.cheat = sim::Cheat::MaxPower,
                .setting = {.key = "cheats.mxpmx",
                            .label = "Power-ups at level 3 (MXPMX)",
                            .category = kCheatsCategory,
                            .changesGameplay = true}},
    CheatToggle{.cheat = sim::Cheat::FastForward,
                .setting = {.key = "cheats.fstfrwrd",
                            .label = "Fast forward (FSTFRWRD)",
                            .category = kCheatsCategory,
                            .changesGameplay = true}},
    CheatToggle{.cheat = sim::Cheat::NoWheels,
                .setting = {.key = "cheats.nwhls",
                            .label = "No wheels (NWHLS)",
                            .category = kCheatsCategory,
                            .changesGameplay = true}},
    CheatToggle{.cheat = sim::Cheat::NoChassis,
                .setting = {.key = "cheats.nchsss",
                            .label = "No chassis (NCHSSS)",
                            .category = kCheatsCategory,
                            .changesGameplay = true}},
    CheatToggle{.cheat = sim::Cheat::NoDriver,
                .setting = {.key = "cheats.ndrvr",
                            .label = "No driver (NDRVR)",
                            .category = kCheatsCategory,
                            .changesGameplay = true}},
};

void DeclareCheatSettings(settings::Registry& registry);

/// A scene that names cheats keeps them; otherwise the licence's session cheats apply.
[[nodiscard]] sim::Cheats RaceCheats(const sim::Cheats& named, const sim::Cheats& session);

/// Enters each toggled plate `session` lacks, in bit order. A later plate's exclusions win.
[[nodiscard]] sim::Cheats WithCheatToggles(sim::Cheats session, const settings::Registry& registry);

}
