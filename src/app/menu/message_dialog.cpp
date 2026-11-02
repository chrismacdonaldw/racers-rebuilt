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
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/ui_text/text_layout.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "DIALOG";
constexpr std::string_view kFrameElement = "dialfrme";
constexpr std::string_view kMessageElement = "dialtext";
constexpr std::string_view kMessageStyle = "menufont";
constexpr std::string_view kButtonStyle = "buttonch";

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

BoxSize SizeOf(const UiImage& image) {
    return {.width = static_cast<float>(image.widthPx),
            .height = static_cast<float>(image.heightPx)};
}

render::Color TintOr(const std::optional<assets::MenuColor>& color) {
    return color ? ToColor(*color) : render::Color{};
}

Result<void> LoadBorder(MenuImageCache& images, const assets::FrameElement& frame,
                        DialogLook& look) {
    for (std::size_t part = 0; part < kFrameParts; ++part) {
        Result<UiImage> image = images.Upload(frame.images.at(part));
        if (!image) {
            return std::unexpected(std::move(image.error()));
        }
        look.border.at(part) = *image;
    }
    look.borderTint = TintOr(frame.color);
    if (frame.hasFillColor != 0) {
        look.fill = TintOr(frame.fillColor);
    }
    return {};
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

Result<void> LoadButton(MenuImageCache& images, const MenuFiles& files, DialogLook& look) {
    return LoadImageButton(images, files, kButtonStyle)
        .transform([&](const ImageButtonLook& button) {
            look.button = button.states;
            look.buttonImage = button.image;
            look.buttonSounds = button.sounds;
        });
}

void PushVisible(const render::SpriteDraw& sprite, std::vector<render::SpriteDraw>& out) {
    if (sprite.bounds.width > 0.0F && sprite.bounds.height > 0.0F) {
        out.push_back(sprite);
    }
}

void DrawFrame(const DialogLook& look, const render::Rect& frame,
               std::vector<render::SpriteDraw>& out) {
    std::array<BoxSize, kFrameParts> sizes{};
    std::ranges::transform(look.border, sizes.begin(), SizeOf);
    const std::array<render::Rect, kFrameParts + 1> slices = FrameSlices(frame, sizes);
    if (look.fill) {
        PushVisible({.bounds = slices.back(), .tint = *look.fill}, out);
    }
    for (std::size_t part = 0; part < kFrameParts; ++part) {
        PushVisible({.texture = look.border.at(part).texture,
                     .bounds = slices.at(part),
                     .tint = look.borderTint},
                    out);
    }
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
        .and_then(
            [&](const assets::FrameElement* frame) { return LoadBorder(images, *frame, look); })
        .and_then([&] { return LoadMessage(*files, look); })
        .and_then([&] { return LoadButton(images, *files, look); })
        .transform([&] { return std::move(look); });
}

void MessageDialog::Tick(const sim::InputFrame& input, const MenuSounds& sounds) {
    if (animation_.Phase() == DialogPhase::Open && input.WasPressed(sim::MenuAction::Confirm)) {
        sounds.Confirm(look_.buttonSounds);
        animation_.Close();
    }
    animation_.Advance(kSimTick);
}

void MessageDialog::Measure(const MenuText& text) {
    const UiFont* font = text.Font(look_.font);
    const UiFont* buttonFont = text.Font(look_.button.at(kFocusedState).font);
    lines_.clear();
    content_.reset();
    if (font == nullptr || buttonFont == nullptr) {
        return;
    }
    lines_ = WrapText(*font, text.Line(messageLine_), look_.wrapWidth);
    content_ = DialogContent{.message = MessageSize(*font, lines_),
                             .buttonImage = look_.buttonImage,
                             .buttonText = {.width = TextWidth(*buttonFont, text.Line(buttonLine_)),
                                            .height = static_cast<float>(buttonFont->heightPx)}};
}

void MessageDialog::Draw(const MenuText& text, std::vector<render::SpriteDraw>& out) const {
    const UiFont* font = text.Font(look_.font);
    if (IsClosed() || !content_ || font == nullptr) {
        return;
    }
    const DialogLayout layout = LayOutDialog(*content_, animation_.Scale());
    DrawFrame(look_, layout.frame, out);
    if (animation_.Phase() != DialogPhase::Open) {
        return;
    }
    const auto lineHeight = static_cast<float>(font->heightPx);
    for (std::size_t line = 0; line < lines_.size(); ++line) {
        LayoutText(*font, lines_[line],
                   {.x = layout.message.x,
                    .y = layout.message.y + (static_cast<float>(line) * lineHeight),
                    .tint = look_.textColor},
                   out);
    }
    const MenuButtonState& button = look_.button.at(kFocusedState);
    out.push_back(
        {.texture = button.image, .bounds = layout.buttonImage, .tint = button.imageTint});
    DrawLabel(text, button, text.Line(buttonLine_), layout.buttonText.x, layout.buttonText.y, out);
}

}
