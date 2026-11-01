#include "sim/hazard/code_puzzle.hpp"

#include <optional>

#include "core/vector3.hpp"

namespace rr::sim {
namespace {

constexpr assets::EventId kFirstRedGate = 200;
constexpr assets::EventId kFirstReveal = 207;
constexpr assets::EventId kRedGateEvent = 21;
constexpr assets::EventId kBlueGateEvent = 30;
constexpr assets::EventId kRedRevealEvent = 29;
constexpr assets::EventId kBlueRevealEvent = 20;
constexpr std::size_t kHeardIds = kCodeGates * 3;
constexpr std::size_t kEventsPerHeard = 2;
constexpr std::size_t kEventsPerSolve = 3;
constexpr Milliseconds kRevealDelay{2'500};
constexpr std::uint8_t kFirstGatePassed = 1;
constexpr std::uint8_t kSecondGatePassed = 3;

assets::EventId RedGate(std::size_t gate) {
    return kFirstRedGate + static_cast<assets::EventId>(gate * 2);
}

bool IsRed(assets::EventId gate) {
    return (gate - kFirstRedGate) % 2 == 0;
}

std::array<assets::EventId, kCodeGates> DrawCode(Random& random) {
    std::array<assets::EventId, kCodeGates> code{};
    for (std::size_t gate = 0; gate < kCodeGates; ++gate) {
        code[gate] = RedGate(gate) + (random.Below(2) == 1 ? 0 : 1);
    }
    return code;
}

std::optional<Vector3> WhereHeard(const TrackEvent& event, const HazardContext& context) {
    if (event.racer) {
        return context.racers[*event.racer].Pose().position;
    }
    return event.position;
}

}

void CodePuzzle::Tick(HazardContext& context) {
    if (revealLeft == Milliseconds{0}) {
        return;
    }
    if (revealLeft <= context.elapsed) {
        revealLeft = Milliseconds{0};
        context.events->Fire({.id = kCodeJingleEvent, .edge = EventEdge::End});
    } else {
        revealLeft -= context.elapsed;
    }
}

void CodePuzzle::Hear(const TrackEvent& event, HazardContext& context) {
    if (event.edge != EventEdge::Start) {
        return;
    }
    const assets::EventId id = event.id;
    bool isWrongGate = false;
    for (std::size_t gate = 0; gate < kCodeGates; ++gate) {
        const assets::EventId red = RedGate(gate);
        isWrongGate = isWrongGate || ((id == red || id == red + 1) && id != code[gate]);
    }
    if (isWrongGate) {
        progress = 0;
    } else if (id == code[0]) {
        progress = kFirstGatePassed;
    } else if (id == code[1] && progress == kFirstGatePassed) {
        progress = kSecondGatePassed;
    } else if (id == code[2] && progress == kSecondGatePassed) {
        Solve(context);
    }
    const std::optional<Vector3> where = WhereHeard(event, context);
    if (id >= kFirstRedGate && id < RedGate(kCodeGates)) {
        context.events->FireStartAndEnd(IsRed(id) ? kRedGateEvent : kBlueGateEvent, where);
    } else if (id >= kFirstReveal && id < kFirstReveal + static_cast<assets::EventId>(kCodeGates)) {
        const assets::EventId shown = code[static_cast<std::size_t>(id - kFirstReveal)];
        context.events->FireStartAndEnd(IsRed(shown) ? kRedRevealEvent : kBlueRevealEvent, where);
    }
}

void CodePuzzle::Solve(HazardContext& context) {
    context.events->FireStartAndEnd(kCodeSolvedEvent, std::nullopt);
    progress = 0;
    revealLeft = kRevealDelay;
    code = DrawCode(*context.random);
    context.events->Fire({.id = kCodeJingleEvent, .edge = EventEdge::Start});
}

std::size_t CodePuzzle::MostEventsPerTick(std::size_t racerCount) {
    return kHeardIds * (racerCount + 1) * (kEventsPerHeard + kEventsPerSolve);
}

CodePuzzle LoadCodePuzzle(Random& random) {
    return {.code = DrawCode(random)};
}

}
