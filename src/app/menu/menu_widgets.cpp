#include "app/menu/menu_widgets.hpp"

#include <format>
#include <utility>
#include <variant>

#include "app/ui_text/text_layout.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPreviousArrow = "arrowlu";
constexpr std::string_view kPreviousArrowFocused = "arrowls";
constexpr std::string_view kNextArrow = "arrowru";
constexpr std::string_view kNextArrowFocused = "arrowrs";
constexpr float kBannerMargin = 20.0F;
constexpr float kHalf = 0.5F;

Result<void> UploadInto(MenuImageCache& images, std::string_view name, UiImage& out) {
    return images.Upload(name).transform([&](const UiImage& image) { out = image; });
}

Result<render::Rect> ArrowAt(const MenuFiles& files, std::string_view screen, MenuPoint selector,
                             std::string_view button, const UiImage& image) {
    return FindMenuElement(files, screen, button).transform([&](const assets::MenuElement* found) {
        const MenuPoint offset = OriginOf(files.layout, *found);
        return SizedAt({.x = selector.x + offset.x, .y = selector.y + offset.y}, image);
    });
}

}

render::Rect SizedAt(MenuPoint origin, const UiImage& image) {
    return {.x = origin.x,
            .y = origin.y,
            .width = static_cast<float>(image.widthPx),
            .height = static_cast<float>(image.heightPx)};
}

render::Rect SpanOf(MenuPoint origin, const assets::MenuRect& rect) {
    return {.x = origin.x,
            .y = origin.y,
            .width = static_cast<float>(rect.right - rect.left + 1),
            .height = static_cast<float>(rect.bottom - rect.top + 1)};
}

Result<const assets::MenuElement*> FindMenuElement(const MenuFiles& files, std::string_view screen,
                                                   std::string_view name) {
    const assets::MenuElement* element = files.layout.Find(name);
    if (element == nullptr) {
        return Fail(ErrorCode::NotFound, std::format("{}.MIB has no {}", screen, name));
    }
    return element;
}

Result<MenuTextStyle> ReadTextStyle(const MenuFiles& files, std::string_view name) {
    const assets::TextStyle* style = files.FindTextStyle(name);
    if (style == nullptr) {
        return Fail(ErrorCode::NotFound, std::format("there's no text style {}", name));
    }
    return MenuTextStyle{.font = style->font,
                         .color = style->color ? ToColor(*style->color) : render::Color{}};
}

Result<ImageButtonLook> LoadImageButton(MenuImageCache& images, const MenuFiles& files,
                                        std::string_view style) {
    const assets::GameButtonStyle* found = files.FindGameButtonStyle(style);
    if (found == nullptr || !found->button.images) {
        return Fail(ErrorCode::NotFound, std::format("there's no button style {}", style));
    }
    ImageButtonLook look{.states = ButtonStates(files, style)};
    for (std::size_t state = 0; state < assets::kControlStates; ++state) {
        Result<UiImage> image = images.Upload(found->button.images->at(state));
        if (!image) {
            return std::unexpected(std::move(image.error()));
        }
        look.states.at(state).image = image->texture;
        if (state == kNormalState) {
            look.image = {.width = static_cast<float>(image->widthPx),
                          .height = static_cast<float>(image->heightPx)};
        }
    }
    return look;
}

Result<SelectorArrows> LoadSelectorArrows(MenuImageCache& images) {
    SelectorArrows arrows;
    return UploadInto(images, kPreviousArrow, arrows.previous.normal)
        .and_then(
            [&] { return UploadInto(images, kPreviousArrowFocused, arrows.previous.focused); })
        .and_then([&] { return UploadInto(images, kNextArrow, arrows.next.normal); })
        .and_then([&] { return UploadInto(images, kNextArrowFocused, arrows.next.focused); })
        .transform([&] { return arrows; });
}

Result<SelectorPlace> PlaceSelector(const MenuFiles& files, std::string_view screen,
                                    std::string_view name, const SelectorArrows& arrows) {
    Result<const assets::MenuElement*> element = FindMenuElement(files, screen, name);
    const auto* selector =
        element ? std::get_if<assets::SelectorElement>(&(*element)->body) : nullptr;
    if (selector == nullptr) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}.MIB {} isn't a selector", screen, name));
    }
    const MenuPoint origin = OriginOf(files.layout, **element);
    Result<render::Rect> previous =
        ArrowAt(files, screen, origin, selector->previousButton, arrows.previous.normal);
    Result<render::Rect> next = previous.and_then([&](const render::Rect&) {
        return ArrowAt(files, screen, origin, selector->nextButton, arrows.next.normal);
    });
    return next.transform([&](const render::Rect& placed) {
        return SelectorPlace{.bounds = SpanOf(origin, (*element)->widget.rect),
                             .previous = *previous,
                             .next = placed};
    });
}

void DrawImageButton(const MenuText& text, const ImageButtonLook& look, std::size_t state,
                     std::u16string_view label, MenuPoint at,
                     std::vector<render::SpriteDraw>& out) {
    const MenuButtonState& shown = look.states.at(state);
    out.push_back(
        {.texture = shown.image,
         .bounds = {.x = at.x, .y = at.y, .width = look.image.width, .height = look.image.height},
         .tint = shown.imageTint});
    const UiFont* font = text.Font(shown.font);
    const float textHeight = font != nullptr ? static_cast<float>(font->heightPx) : 0.0F;
    DrawLabel(text, shown, label, at.x + look.image.width,
              at.y + ((look.image.height - textHeight) * kHalf), out);
}

void DrawSelectorArrows(const SelectorArrows& arrows, const SelectorPlace& place, bool isFocused,
                        std::vector<render::SpriteDraw>& out) {
    out.push_back(
        {.texture = (isFocused ? arrows.previous.focused : arrows.previous.normal).texture,
         .bounds = place.previous});
    out.push_back({.texture = (isFocused ? arrows.next.focused : arrows.next.normal).texture,
                   .bounds = place.next});
}

void DrawBanner(const MenuText& text, const MenuTextStyle& style, std::u16string_view title,
                const MenuPicture& backdrop, float top, std::vector<render::SpriteDraw>& out) {
    if (const UiFont* font = text.Font(style.font)) {
        const float right = backdrop.bounds.x + backdrop.bounds.width - kBannerMargin;
        LayoutText(*font, title,
                   {.x = right - TextWidth(*font, title), .y = top, .tint = style.color}, out);
    }
}

}
