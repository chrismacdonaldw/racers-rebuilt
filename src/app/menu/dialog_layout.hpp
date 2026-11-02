#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/duration.hpp"
#include "render/render_view.hpp"

namespace rr::app {

inline constexpr std::size_t kFrameParts = 8;
inline constexpr Milliseconds kDialogResize{300};

enum class DialogPhase : std::uint8_t { Opening, Open, Closing, Closed };

class DialogAnimation {
public:
    [[nodiscard]] DialogPhase Phase() const { return phase_; }

    /// The share of its full size the box is drawn at, from 0 to 1.
    [[nodiscard]] float Scale() const;

    void Advance(Milliseconds elapsed);

    /// Only an open dialog starts closing.
    void Close();

private:
    DialogPhase phase_ = DialogPhase::Opening;
    Milliseconds phaseTime_{};
};

struct BoxSize {
    float width = 0.0F;
    float height = 0.0F;

    friend bool operator==(const BoxSize&, const BoxSize&) = default;
};

struct DialogButtonSize {
    BoxSize image;
    BoxSize text;
};

struct DialogContent {
    BoxSize message;
    std::span<const DialogButtonSize> buttons;
};

struct DialogButtonRects {
    render::Rect image;
    render::Rect text;
};

struct DialogLayout {
    BoxSize halfFrame;
    render::Rect message;
    std::vector<DialogButtonRects> buttons;
};

/// Centred on the UI canvas, with the buttons stacked under the message.
[[nodiscard]] DialogLayout LayOutDialog(const DialogContent& content);

/// `scale` shrinks the frame about the centre in whole units, as the box grows and shrinks.
[[nodiscard]] render::Rect DialogFrame(const DialogLayout& layout, float scale);

/// The border parts go clockwise from the top left corner. The last rect is the fill.
[[nodiscard]] std::array<render::Rect, kFrameParts + 1> FrameSlices(
    const render::Rect& frame, const std::array<BoxSize, kFrameParts>& parts);

}
