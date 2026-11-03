#include "sim/race/cheats.hpp"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <ranges>

namespace rr::sim {
namespace {

struct CheatPlate {
    Cheat cheat;
    std::string_view plate;
};

constexpr std::array kPlates{
    CheatPlate{.cheat = Cheat::IgnoreSurfaces, .plate = "NSLWJ"},
    CheatPlate{.cheat = Cheat::HoverTurbo, .plate = "FLYSKYHGH"},
    CheatPlate{.cheat = Cheat::RedBricks, .plate = "PGLLRD"},
    CheatPlate{.cheat = Cheat::YellowBricks, .plate = "PGLLYLL"},
    CheatPlate{.cheat = Cheat::GreenBricks, .plate = "PGLLGRN"},
    CheatPlate{.cheat = Cheat::MirroredBoss, .plate = "LNFRRRM"},
    CheatPlate{.cheat = Cheat::RedOnly, .plate = "RPCRNLY"},
    CheatPlate{.cheat = Cheat::MaxPower, .plate = "MXPMX"},
    CheatPlate{.cheat = Cheat::FastForward, .plate = "FSTFRWRD"},
    CheatPlate{.cheat = Cheat::NoWheels, .plate = "NWHLS"},
    CheatPlate{.cheat = Cheat::NoChassis, .plate = "NCHSSS"},
    CheatPlate{.cheat = Cheat::NoDriver, .plate = "NDRVR"},
};

constexpr std::string_view kClearPlate = "NMRCHTS";
constexpr char kPlateSeparator = ',';

}

bool Cheats::EnterPlate(std::string_view plate) {
    if (plate == kClearPlate) {
        bits_ = 0;
        return true;
    }
    const auto entry = std::ranges::find(kPlates, plate, &CheatPlate::plate);
    if (entry == kPlates.end()) {
        return false;
    }
    const Cheat cheat = entry->cheat;
    if (Has(cheat)) {
        Clear(cheat);
        return true;
    }
    bits_ |= static_cast<std::uint16_t>(cheat);
    const auto clearAll = [this](std::initializer_list<Cheat> cheats) {
        for (const Cheat other : cheats) {
            Clear(other);
        }
    };
    const auto clearLastPart = [this](Cheat kept, Cheat cleared) {
        if (Has(kept) && Has(cleared)) {
            Clear(cleared);
        }
    };
    switch (cheat) {
        case Cheat::RedBricks:
            clearAll({Cheat::YellowBricks, Cheat::GreenBricks, Cheat::RedOnly});
            break;
        case Cheat::YellowBricks:
            clearAll({Cheat::RedBricks, Cheat::GreenBricks, Cheat::RedOnly});
            break;
        case Cheat::GreenBricks:
            clearAll({Cheat::RedBricks, Cheat::YellowBricks, Cheat::RedOnly});
            break;
        case Cheat::RedOnly:
            clearAll({Cheat::RedBricks, Cheat::YellowBricks, Cheat::GreenBricks, Cheat::MaxPower});
            break;
        case Cheat::MaxPower:
            clearAll({Cheat::RedOnly});
            break;
        case Cheat::NoWheels:
            clearLastPart(Cheat::NoChassis, Cheat::NoDriver);
            break;
        case Cheat::NoChassis:
            clearLastPart(Cheat::NoDriver, Cheat::NoWheels);
            break;
        case Cheat::NoDriver:
            clearLastPart(Cheat::NoWheels, Cheat::NoChassis);
            break;
        default:
            break;
    }
    return true;
}

std::string_view PlateOf(Cheat cheat) {
    return std::ranges::find(kPlates, cheat, &CheatPlate::cheat)->plate;
}

std::string CheatPlates(const Cheats& cheats) {
    std::string text;
    for (const CheatPlate& entry : kPlates) {
        if (!cheats.Has(entry.cheat)) {
            continue;
        }
        if (!text.empty()) {
            text += kPlateSeparator;
        }
        text += entry.plate;
    }
    return text;
}

std::optional<Cheats> ParseCheatPlates(std::string_view text) {
    std::uint16_t bits = 0;
    for (const auto part : std::views::split(text, kPlateSeparator)) {
        const auto entry = std::ranges::find(kPlates, std::string_view(part), &CheatPlate::plate);
        const auto bit = entry == kPlates.end() ? 0U : static_cast<unsigned>(entry->cheat);
        if (bit == 0 || (bits & bit) != 0) {
            return std::nullopt;
        }
        bits = static_cast<std::uint16_t>(bits | bit);
    }
    return Cheats(bits);
}

}
