#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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

struct DialogContent {
    BoxSize message;
    BoxSize buttonImage;
    BoxSize buttonText;
};

struct DialogLayout {
    render::Rect frame;
    render::Rect message;
    render::Rect buttonImage;
    render::Rect buttonText;
};

/// Centred on the UI canvas. `scale` shrinks only the frame.
[[nodiscard]] DialogLayout LayOutDialog(const DialogContent& content, float scale);

/// The border parts go clockwise from the top left corner. The last rect is the fill.
[[nodiscard]] std::array<render::Rect, kFrameParts + 1> FrameSlices(
    const render::Rect& frame, const std::array<BoxSize, kFrameParts>& parts);

}
