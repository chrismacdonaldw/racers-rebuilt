#include "app/menu/message_dialog.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

#include "app/frame_loop.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/ui_text/text_layout.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "DIALOG";
constexpr std::string_view kFrameElement = "dialfrme";
constexpr std::string_view kMessageElement = "dialtext";
constexpr std::string_view kMessageStyle = "menufont";
constexpr std::array<std::string_view, static_cast<std::size_t>(DialogButtonStyle::Count)>
    kButtonStyles{"buttonch", "buttonca"};

template <typename Body>
Result<const Body*> FindElement(const assets::MenuLayout& layout, std::string_view name) {
    const auto element = std::ranges::find(layout.elements, name, &assets::MenuElement::name);
    const Body* body =
        element == layout.elements.end() ? nullptr : std::get_if<Body>(&element->body);
    if (body == nullptr) {
        return Fail(ErrorCode::NotFound, std::format("{}.MIB has no {}", kScreen, name));
    }
    return body;
}

render::Color TintOr(const std::optional<assets::MenuColor>& color) {
    return color ? ToColor(*color) : render::Color{};
}

Result<void> LoadMessage(const MenuFiles& files, DialogLook& look) {
    const assets::TextStyle* style = files.FindTextStyle(kMessageStyle);
    if (style == nullptr) {
        return Fail(ErrorCode::NotFound, std::format("there's no text style {}", kMessageStyle));
    }
    return FindElement<assets::TextLabelElement>(files.layout, kMessageElement)
        .transform([&](const assets::TextLabelElement* label) {
            look.font = label->font.value_or(style->font);
            look.textColor = TintOr(label->color ? label->color : style->color);
            look.wrapWidth = static_cast<float>(
                std::max(0, label->wrapWidth.value_or(style->wrapWidth.value_or(0))));
        });
}

Result<void> LoadButtons(MenuImageCache& images, const MenuFiles& files, DialogLook& look) {
    for (std::size_t style = 0; style < kButtonStyles.size(); ++style) {
        Result<ImageButtonLook> button = LoadImageButton(images, files, kButtonStyles.at(style));
        if (!button) {
            return std::unexpected(std::move(button.error()));
        }
        look.buttons.at(style) = *button;
    }
    return {};
}

BoxSize MessageSize(const UiFont& font, std::span<const std::u16string_view> lines) {
    BoxSize size{.height = static_cast<float>(lines.size() * font.heightPx)};
    for (const std::u16string_view line : lines) {
        size.width = std::max(size.width, TextWidth(font, line));
    }
    return size;
}

}

Result<DialogLook> LoadDialogLook(render::Renderer& renderer, const assets::Vfs& vfs) {
    Result<MenuFiles> files = ReadMenuFiles(vfs, kScreen);
    if (!files) {
        return std::unexpected(std::move(files.error()));
    }
    DialogLook look;
    MenuImageCache images(renderer, vfs, *files);
    return FindElement<assets::FrameElement>(files->layout, kFrameElement)
        .and_then([&](const assets::FrameElement* frame) { return LoadMenuFrame(images, *frame); })
        .transform([&](const MenuFrameLook& frame) { look.frame = frame; })
        .and_then([&] { return LoadMessage(*files, look); })
        .and_then([&] { return LoadButtons(images, *files, look); })
        .transform([&] { return std::move(look); });
}

MessageDialog::MessageDialog(DialogLook look, std::size_t messageLine,
                             std::vector<DialogButton> buttons)
    : look_(std::move(look)),
      messageLine_(messageLine),
      buttons_(std::move(buttons)),
      focus_(buttons_.empty() ? 0 : buttons_.size() - 1) {}

std::optional<std::size_t> MessageDialog::Tick(const sim::InputFrame& input,
                                               const MenuSounds& sounds) {
    std::optional<std::size_t> pressed;
    if (animation_.Phase() == DialogPhase::Open && !buttons_.empty()) {
        const PointerChoice pointed = pointer_.Tick(input.pointer, buttonTargets_);
        MoveFocus(input, sounds, pointed.Focus());
        if (input.WasPressed(sim::MenuAction::Confirm) ||
            pointed.Clicks(sim::MenuAction::Confirm)) {
            sounds.Confirm(LookOf(buttons_[focus_]).sounds);
            animation_.Close();
            pressed = focus_;
        }
    }
    animation_.Advance(kSimTick);
    return pressed;
}

void MessageDialog::Measure(const MenuText& text) {
    lines_.clear();
    layout_.reset();
    buttonTargets_.clear();
    const UiFont* font = text.Font(look_.font);
    if (font == nullptr) {
        return;
    }
    std::vector<DialogButtonSize> sizes;
    for (const DialogButton& button : buttons_) {
        const ImageButtonLook& look = LookOf(button);
        const UiFont* buttonFont = text.Font(look.states.at(kFocusedState).font);
        if (buttonFont == nullptr) {
            return;
        }
        sizes.push_back({.image = look.image,
                         .text = {.width = TextWidth(*buttonFont, text.Line(button.line)),
                                  .height = static_cast<float>(buttonFont->heightPx)}});
    }
    lines_ = WrapText(*font, text.Line(messageLine_), look_.wrapWidth);
    layout_ = LayOutDialog({.message = MessageSize(*font, lines_), .buttons = sizes});
    for (std::size_t i = 0; i < layout_->buttons.size(); ++i) {
        const DialogButtonRects& rects = layout_->buttons[i];
        buttonTargets_.push_back({.area = Spanning(rects.image, rects.text), .control = i});
    }
}

void MessageDialog::Draw(const MenuText& text, std::vector<render::SpriteDraw>& out) const {
    const UiFont* font = text.Font(look_.font);
    if (IsClosed() || !layout_ || font == nullptr) {
        return;
    }
    DrawMenuFrame(look_.frame, DialogFrame(*layout_, animation_.Scale()), out);
    if (animation_.Phase() != DialogPhase::Open) {
        return;
    }
    const auto lineHeight = static_cast<float>(font->heightPx);
    for (std::size_t line = 0; line < lines_.size(); ++line) {
        LayoutText(*font, lines_[line],
                   {.x = layout_->message.x,
                    .y = layout_->message.y + (static_cast<float>(line) * lineHeight),
                    .tint = look_.textColor},
                   out);
    }
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        const MenuButtonState& state =
            LookOf(buttons_[i]).states.at(i == focus_ ? kFocusedState : kNormalState);
        const DialogButtonRects& rects = layout_->buttons.at(i);
        out.push_back({.texture = state.image, .bounds = rects.image, .tint = state.imageTint});
        DrawLabel(text, state, text.Line(buttons_[i].line), rects.text.x, rects.text.y, out);
    }
}

const ImageButtonLook& MessageDialog::LookOf(const DialogButton& button) const {
    return look_.buttons.at(static_cast<std::size_t>(button.style));
}

void MessageDialog::MoveFocus(const sim::InputFrame& input, const MenuSounds& sounds,
                              std::optional<std::size_t> pointed) {
    const std::size_t count = buttons_.size();
    std::size_t next = pointed.value_or(focus_);
    if (input.WasPressed(sim::MenuAction::Up)) {
        next = (next + count - 1) % count;
    }
    if (input.WasPressed(sim::MenuAction::Down)) {
        next = (next + 1) % count;
    }
    if (next != focus_) {
        sounds.MoveHighlight(LookOf(buttons_[focus_]).sounds, LookOf(buttons_[next]).sounds);
        focus_ = next;
    }
}

}
