#include "app/menu/menu_page.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "app/ui_text/text_layout.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/game_path.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kTextButtonStyle = "nubutton";
constexpr std::size_t kFirstImageColor = 1;
constexpr std::size_t kFirstTextColor = 7;
constexpr float kChannelMax = 255.0F;

const assets::MenuElement* FindById(const assets::MenuLayout& layout, std::int32_t id) {
    const auto element = std::ranges::find(layout.elements, id, &assets::MenuElement::id);
    return element == layout.elements.end() ? nullptr : &*element;
}

render::Rect BoundsOf(const assets::MenuRect& rect, MenuPoint origin, const UiImage& image) {
    const bool takesImageSize = rect.right == 0 && rect.bottom == 0;
    return {.x = origin.x,
            .y = origin.y,
            .width = takesImageSize ? static_cast<float>(image.widthPx)
                                    : static_cast<float>(rect.right - rect.left + 1),
            .height = takesImageSize ? static_cast<float>(image.heightPx)
                                     : static_cast<float>(rect.bottom - rect.top + 1)};
}

template <std::size_t N>
std::string_view PickName(const std::optional<std::array<std::string, N>>& own,
                          const std::optional<std::array<std::string, N>>& style,
                          std::size_t state) {
    if (own && !(*own)[state].empty()) {
        return (*own)[state];
    }
    return style ? std::string_view((*style)[state]) : std::string_view{};
}

assets::GameButtonElement StyleNamed(const MenuFiles& files, std::string_view name) {
    const assets::GameButtonStyle* style = files.FindGameButtonStyle(name);
    return style != nullptr ? style->button : assets::GameButtonElement{};
}

render::Color ColorAt(const decltype(assets::GameButtonElement::colors)& colors,
                      std::size_t index) {
    return colors ? ToColor(colors->at(index)) : render::Color{};
}

MenuButtonState StateOf(const assets::GameButtonElement& own,
                        const assets::GameButtonElement& style, std::size_t state) {
    const auto& colors = own.colors ? own.colors : style.colors;
    return {.imageTint = ColorAt(colors, kFirstImageColor + state),
            .font = std::string(PickName(own.fonts, style.fonts, state)),
            .textColor = ColorAt(colors, kFirstTextColor + state)};
}

class PageBuilder {
public:
    PageBuilder(render::Renderer& renderer, const assets::Vfs& vfs, const MenuFiles& files)
        : files_(&files),
          images_(renderer, vfs, files),
          style_(StyleNamed(files, kTextButtonStyle)) {}

    Result<void> Add(const assets::MenuElement& element, std::span<const ButtonLine> labels) {
        const MenuPoint origin = OriginOf(files_->layout, element);
        if (const auto* image = std::get_if<assets::ImageElement>(&element.body)) {
            return AddPicture(element, *image, origin);
        }
        const auto* button = std::get_if<assets::GameButtonElement>(&element.body);
        const auto line =
            std::ranges::find(labels, std::string_view(element.name), &ButtonLine::button);
        if (button == nullptr || line == labels.end()) {
            return {};
        }
        return AddButton(element, *button, origin, line->line);
    }

    [[nodiscard]] MenuPage& Page() { return page_; }

private:
    Result<void> AddPicture(const assets::MenuElement& element, const assets::ImageElement& image,
                            MenuPoint origin) {
        const assets::ImageStyle* style = files_->FindImageStyle(element.name);
        const std::string_view name =
            !image.image.empty() || style == nullptr ? std::string_view(image.image) : style->image;
        std::optional<assets::MenuColor> color = image.color;
        if (!color && style != nullptr) {
            color = style->color;
        }
        return images_.Upload(name).transform([&](const UiImage& uploaded) {
            page_.pictures.push_back({.texture = uploaded.texture,
                                      .bounds = BoundsOf(element.widget.rect, origin, uploaded),
                                      .tint = color ? ToColor(*color) : render::Color{}});
        });
    }

    Result<void> AddButton(const assets::MenuElement& element, const assets::GameButtonElement& own,
                           MenuPoint origin, std::size_t textLine) {
        MenuButton button{.name = element.name, .textLine = textLine};
        for (std::size_t state = 0; state < assets::kControlStates; ++state) {
            Result<UiImage> image = images_.Upload(PickName(own.images, style_.images, state));
            if (!image) {
                return std::unexpected(std::move(image.error()));
            }
            if (state == kNormalState) {
                button.bounds = BoundsOf(element.widget.rect, origin, *image);
            }
            button.states.at(state) = StateOf(own, style_, state);
            button.states.at(state).image = image->texture;
        }
        page_.buttons.push_back(std::move(button));
        return {};
    }

    const MenuFiles* files_;
    MenuImageCache images_;
    assets::GameButtonElement style_;
    MenuPage page_;
};

}

MenuPoint OriginOf(const assets::MenuLayout& layout, const assets::MenuElement& element) {
    MenuPoint origin;
    const assets::MenuElement* at = &element;
    for (std::size_t depth = 0; at != nullptr && depth <= layout.elements.size(); ++depth) {
        origin.x += static_cast<float>(at->widget.rect.left);
        origin.y += static_cast<float>(at->widget.rect.top);
        at = FindById(layout, at->parentId);
    }
    return origin;
}

Result<UiImage> MenuImageCache::Upload(std::string_view name) {
    const std::string key = assets::ToUpperAscii(name);
    if (const auto image = images_.find(key); image != images_.end()) {
        return image->second;
    }
    const assets::ImageListEntry* entry = files_->FindImage(name);
    if (entry == nullptr) {
        return Fail(ErrorCode::NotFound,
                    std::format("menu image {} isn't in any image list", name));
    }
    return LoadUiImage(*renderer_, *vfs_, kMenuFolder, *entry).transform([&](const UiImage& image) {
        images_.emplace(key, image);
        return image;
    });
}

render::Color ToColor(const assets::MenuColor& color) {
    return {.r = static_cast<float>(color.red) / kChannelMax,
            .g = static_cast<float>(color.green) / kChannelMax,
            .b = static_cast<float>(color.blue) / kChannelMax,
            .a = static_cast<float>(color.alpha) / kChannelMax};
}

Result<MenuPage> BuildMenuPage(render::Renderer& renderer, const assets::Vfs& vfs,
                               const MenuFiles& files, std::span<const ButtonLine> labels) {
    PageBuilder builder(renderer, vfs, files);
    for (const assets::MenuElement& element : files.layout.elements) {
        Result<void> added = builder.Add(element, labels);
        if (!added) {
            return std::unexpected(std::move(added.error()));
        }
    }
    return std::move(builder.Page());
}

std::array<MenuButtonState, assets::kControlStates> ButtonStates(const MenuFiles& files,
                                                                 std::string_view style) {
    const assets::GameButtonElement element = StyleNamed(files, style);
    std::array<MenuButtonState, assets::kControlStates> states;
    for (std::size_t state = 0; state < states.size(); ++state) {
        states.at(state) = StateOf({}, element, state);
    }
    return states;
}

std::array<MenuButtonState, assets::kControlStates> TextButtonStates(const MenuFiles& files) {
    return ButtonStates(files, kTextButtonStyle);
}

void DrawPictures(std::span<const MenuPicture> pictures, std::vector<render::SpriteDraw>& out) {
    for (const MenuPicture& picture : pictures) {
        out.push_back({.texture = picture.texture, .bounds = picture.bounds, .tint = picture.tint});
    }
}

void DrawLabel(const MenuText& text, const MenuButtonState& state, std::u16string_view label,
               float x, float y, std::vector<render::SpriteDraw>& out) {
    if (const UiFont* font = text.Font(state.font)) {
        LayoutText(*font, label, {.x = x, .y = y, .tint = state.textColor}, out);
    }
}

}
