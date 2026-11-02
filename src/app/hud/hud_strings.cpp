#include "app/hud/hud_strings.hpp"

#include <cstddef>
#include <format>

namespace rr::app {
namespace {

constexpr std::size_t kResume = 0x0E;
constexpr std::size_t kRestart = 0x0F;
constexpr std::size_t kRestartCircuit = 0x10;
constexpr std::size_t kQuit = 0x11;
constexpr std::size_t kYes = 0x12;
constexpr std::size_t kNo = 0x13;
constexpr std::size_t kPaused = 0x14;
constexpr std::size_t kRecordStands = 0x15;
constexpr std::size_t kRecordBeaten = 0x17;
constexpr std::size_t kFirstPlaceSuffix = 0x19;
constexpr std::size_t kGap = 0x21;
constexpr std::size_t kWrongWay = 0x24;
constexpr std::size_t kBest = 0x25;
constexpr std::size_t kTotal = 0x26;
constexpr std::size_t kLap = 0x27;
constexpr std::size_t kFinish = 0x28;
constexpr std::size_t kGo = 0x29;
constexpr std::size_t kFinished = 0x2A;
constexpr std::size_t kStillRacing = 0x2B;
constexpr std::size_t kSure = 0x2C;
constexpr std::size_t kCircuit = 0x2D;

}

Result<HudStrings> ReadHudStrings(std::span<const std::u16string> gameStrings) {
    if (gameStrings.size() <= kCircuit) {
        return Fail(ErrorCode::InvalidData,
                    std::format("game strings hold {} entries, the HUD needs {}",
                                gameStrings.size(), kCircuit + 1));
    }
    HudStrings strings{
        .gap = gameStrings[kGap],
        .wrongWay = gameStrings[kWrongWay],
        .best = gameStrings[kBest],
        .total = gameStrings[kTotal],
        .lap = gameStrings[kLap],
        .finish = gameStrings[kFinish],
        .go = gameStrings[kGo],
        .finished = gameStrings[kFinished],
        .stillRacing = gameStrings[kStillRacing],
        .circuit = gameStrings[kCircuit],
        .paused = gameStrings[kPaused],
        .resume = gameStrings[kResume],
        .restart = gameStrings[kRestart],
        .restartCircuit = gameStrings[kRestartCircuit],
        .quit = gameStrings[kQuit],
        .sure = gameStrings[kSure],
        .yes = gameStrings[kYes],
        .no = gameStrings[kNo],
        .recordStands = {gameStrings[kRecordStands], gameStrings[kRecordStands + 1]},
        .recordBeaten = {gameStrings[kRecordBeaten], gameStrings[kRecordBeaten + 1]}};
    for (std::size_t i = 0; i < kShownPlaces; ++i) {
        strings.placeSuffixes[i] = gameStrings[kFirstPlaceSuffix + i];
    }
    return strings;
}

}
