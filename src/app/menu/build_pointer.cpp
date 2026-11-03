#include "app/menu/build_pointer.hpp"

#include <cstdint>

#include "app/frame_loop.hpp"

namespace rr::app {
namespace {

constexpr auto kFirstButtonRow = static_cast<std::size_t>(BuildRow::Move);
constexpr Milliseconds kFirstRepeat{500};
constexpr Milliseconds kRepeat{250};
constexpr std::int32_t kMoveSpots = 8;

bool Contains(const render::Rect& area, const sim::MenuPointer& pointer) {
    return pointer.isOver && pointer.x >= area.x && pointer.y >= area.y &&
           pointer.x < area.x + area.width && pointer.y < area.y + area.height;
}

std::optional<PadPress> SpotOn(const CarBuildLook& look, BuildPad pad,
                               const sim::MenuPointer& pointer) {
    const std::vector<PadSpot>& spots = look.pads.at(static_cast<std::size_t>(pad)).spots;
    for (std::size_t spot = 0; spot < spots.size(); ++spot) {
        if (Contains(spots[spot].area, pointer)) {
            return PadPress{.pad = pad, .spot = spot};
        }
    }
    return std::nullopt;
}

std::optional<PadPress> SpotAt(const CarBuildLook& look, const sim::MenuPointer& pointer) {
    for (std::size_t pad = 0; pad < kBuildPads; ++pad) {
        if (std::optional<PadPress> hit = SpotOn(look, static_cast<BuildPad>(pad), pointer)) {
            return hit;
        }
    }
    return std::nullopt;
}

BuildRequest ViewRequest(std::int32_t spot) {
    switch (spot) {
        case 1:
            return {.command = BuildCommand::TiltUp};
        case 3:
            return {.command = BuildCommand::TurnRight};
        case 5:
            return {.command = BuildCommand::TiltDown};
        case 7:
            return {.command = BuildCommand::TurnLeft};
        case 9:
            return {.command = BuildCommand::ResetView};
        default:
            return {};
    }
}

}

BuildRequest PadRequest(BuildPad pad, std::int32_t spot) {
    if (pad == BuildPad::View) {
        return ViewRequest(spot);
    }
    if (spot < 1 || spot > kMoveSpots) {
        return {};
    }
    return {.command = BuildCommand::Move, .direction = static_cast<std::size_t>(spot - 1)};
}

BuildPointing BuildPointer::Tick(const sim::InputFrame& input, const CarBuildLook& look,
                                 CarBuildMenu& menu) {
    targets_.clear();
    AddSelectorTargets(look.sets, static_cast<std::size_t>(BuildRow::Sets), targets_);
    AddSelectorTargets(look.pieces, static_cast<std::size_t>(BuildRow::Pieces), targets_);
    for (std::size_t button = 0; button < kBuildButtons; ++button) {
        targets_.push_back({.area = look.buttons.at(button).bounds,
                            .control = kFirstButtonRow + button,
                            .click = button < kBuildPads
                                         ? std::nullopt
                                         : std::optional(sim::MenuAction::Confirm)});
    }
    PointerChoice choice = buttons_.Tick(input.pointer, targets_);
    if (const std::optional<std::size_t> focus = choice.Focus();
        focus && !menu.FocusOn(static_cast<BuildRow>(*focus))) {
        choice = {};
    }
    BuildPointing pointing{.input = WithClick(input, choice)};
    if (!menu.Held()) {
        pointing.pad = Press(input.pointer, look);
    }
    if (input.pointer.released) {
        press_.reset();
    }
    return pointing;
}

void BuildPointer::TickIdle(const sim::MenuPointer& pointer) {
    if (pointer.released) {
        press_.reset();
    }
}

BuildRequest BuildPointer::Press(const sim::MenuPointer& pointer, const CarBuildLook& look) {
    if (pointer.pressed) {
        press_ = SpotAt(look, pointer);
        repeatIn_ = kFirstRepeat;
        return Fired(look);
    }
    if (!press_) {
        return {};
    }
    repeatIn_ -= kSimTick;
    if (repeatIn_ > Milliseconds{0}) {
        return {};
    }
    repeatIn_ = kRepeat;
    const std::optional<PadPress> under = SpotOn(look, press_->pad, pointer);
    if (!under) {
        return {};
    }
    press_ = under;
    return Fired(look);
}

BuildRequest BuildPointer::Fired(const CarBuildLook& look) const {
    if (!press_) {
        return {};
    }
    const PadLook& pad = look.pads.at(static_cast<std::size_t>(press_->pad));
    return PadRequest(press_->pad, pad.spots.at(press_->spot).id);
}

void BuildPointer::DrawPressed(const CarBuildLook& look,
                               std::vector<render::SpriteDraw>& sprites) const {
    if (!press_) {
        return;
    }
    const PadLook& pad = look.pads.at(static_cast<std::size_t>(press_->pad));
    const render::Rect& area = pad.spots.at(press_->spot).area;
    const auto width = static_cast<float>(pad.highlight.widthPx);
    const auto height = static_cast<float>(pad.highlight.heightPx);
    if (width <= 0.0F || height <= 0.0F) {
        return;
    }
    sprites.push_back({.texture = pad.highlight.texture,
                       .bounds = area,
                       .uv = {.x = (area.x - pad.origin.x) / width,
                              .y = (area.y - pad.origin.y) / height,
                              .width = area.width / width,
                              .height = area.height / height}});
}

}
