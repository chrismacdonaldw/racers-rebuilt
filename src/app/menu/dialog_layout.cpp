#include "app/menu/dialog_layout.hpp"

#include <algorithm>
#include <cmath>

#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr float kMessageMargin = 20.0F;
constexpr float kButtonGap = 20.0F;
constexpr float kButtonLeft = 25.0F;

float Half(float length) {
    return length * 0.5F;
}

render::Rect Offset(const render::Rect& rect, float x, float y) {
    return {.x = rect.x + x, .y = rect.y + y, .width = rect.width, .height = rect.height};
}

}

float DialogAnimation::Scale() const {
    const float share =
        static_cast<float>(phaseTime_.count()) / static_cast<float>(kDialogResize.count());
    switch (phase_) {
        case DialogPhase::Opening:
            return std::min(share, 1.0F);
        case DialogPhase::Open:
            return 1.0F;
        case DialogPhase::Closing:
            return std::max(1.0F - share, 0.0F);
        case DialogPhase::Closed:
            break;
    }
    return 0.0F;
}

void DialogAnimation::Advance(Milliseconds elapsed) {
    if (phase_ != DialogPhase::Opening && phase_ != DialogPhase::Closing) {
        return;
    }
    phaseTime_ += elapsed;
    if (phaseTime_ > kDialogResize) {
        phase_ = phase_ == DialogPhase::Opening ? DialogPhase::Open : DialogPhase::Closed;
        phaseTime_ = {};
    }
}

void DialogAnimation::Close() {
    if (phase_ == DialogPhase::Open) {
        phase_ = DialogPhase::Closing;
        phaseTime_ = {};
    }
}

DialogLayout LayOutDialog(const DialogContent& content) {
    BoxSize half{.width = Half(content.message.width + (2.0F * kMessageMargin)),
                 .height = Half(content.message.height + (2.0F * kMessageMargin))};
    for (const DialogButtonSize& button : content.buttons) {
        half.height += Half(button.image.height + kButtonGap);
    }
    const float left = Half(render::kUiWidth) - std::trunc(half.width);
    const float top = Half(render::kUiHeight) - std::trunc(half.height);
    DialogLayout layout{.halfFrame = half,
                        .message = {.x = left + kMessageMargin,
                                    .y = top + kMessageMargin,
                                    .width = content.message.width,
                                    .height = content.message.height}};
    float buttonTop = top + content.message.height + (2.0F * kMessageMargin);
    for (const auto& [image, text] : content.buttons) {
        const float textInset =
            image.height > text.height ? std::floor(Half(image.height - text.height)) : 0.0F;
        const render::Rect button{
            .x = left + kButtonLeft, .y = buttonTop, .width = image.width, .height = image.height};
        layout.buttons.push_back({.image = button,
                                  .text = {.x = button.x + image.width,
                                           .y = button.y + textInset,
                                           .width = text.width,
                                           .height = text.height}});
        buttonTop += image.height + kButtonGap;
    }
    return layout;
}

render::Rect DialogFrame(const DialogLayout& layout, float scale) {
    const float scaledX = std::trunc(layout.halfFrame.width * scale);
    const float scaledY = std::trunc(layout.halfFrame.height * scale);
    return {.x = Half(render::kUiWidth) - scaledX,
            .y = Half(render::kUiHeight) - scaledY,
            .width = 2.0F * scaledX,
            .height = 2.0F * scaledY};
}

std::array<render::Rect, kFrameParts + 1> FrameSlices(
    const render::Rect& frame, const std::array<BoxSize, kFrameParts>& parts) {
    const auto& [topLeft, top, topRight, right, bottomRight, bottom, bottomLeft, left] = parts;
    const float width = frame.width;
    const float height = frame.height;
    const float innerRight = width - topRight.width;
    const float innerBottom = height - bottomLeft.height;
    const std::array<render::Rect, kFrameParts + 1> slices{{
        {.width = topLeft.width, .height = topLeft.height},
        {.x = topLeft.width, .width = innerRight - topLeft.width, .height = top.height},
        {.x = innerRight, .width = topRight.width, .height = topLeft.height},
        {.x = width - right.width,
         .y = topLeft.height,
         .width = right.width,
         .height = height - bottomRight.height - topLeft.height},
        {.x = width - bottomRight.width,
         .y = height - bottomRight.height,
         .width = bottomRight.width,
         .height = bottomRight.height},
        {.x = bottomLeft.width,
         .y = height - bottom.height,
         .width = width - bottomRight.width - bottomLeft.width,
         .height = bottom.height},
        {.y = innerBottom, .width = bottomLeft.width, .height = bottomLeft.height},
        {.y = topLeft.height, .width = left.width, .height = innerBottom - topLeft.height},
        {.x = topLeft.width,
         .y = topLeft.height,
         .width = innerRight - topLeft.width,
         .height = innerBottom - topLeft.height},
    }};
    std::array<render::Rect, kFrameParts + 1> placed{};
    std::ranges::transform(slices, placed.begin(), [&](const render::Rect& slice) {
        return Offset(slice, frame.x, frame.y);
    });
    return placed;
}

}
