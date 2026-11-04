#include "app/menu/menu_page.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "app/menu/menu_sounds.hpp"
#include "app/menu/side_tiles.hpp"
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
          style_(StyleNamed(files, kTextButtonStyle)),
          styleSounds_(GameButtonSounds(files, kTextButtonStyle)) {}

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
        button.sounds = element.control && element.control->soundIds ? *element.control->soundIds
                                                                     : styleSounds_;
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
    assets::SoundIds styleSounds_;
    MenuPage page_;
};

void AppendPiece(const MenuPicture& picture, const render::Rect& piece,
                 std::vector<render::SpriteDraw>& out) {
    const render::Rect& bounds = picture.bounds;
    if (piece.width <= 0.0F || piece.height <= 0.0F) {
        return;
    }
    out.push_back({.texture = picture.texture,
                   .bounds = piece,
                   .uv = PieceUv(bounds, piece),
                   .tint = picture.tint});
}

struct Uncut {
    render::Rect piece;
    std::size_t hole = 0;
};

constexpr std::size_t kMaxHoles = 8;
constexpr std::size_t kSidesPerCut = 4;

std::array<render::Rect, kSidesPerCut> SidesAround(const render::Rect& piece,
                                                   const render::Rect& hole) {
    const float right = piece.x + piece.width;
    const float bottom = piece.y + piece.height;
    const float holeLeft = std::clamp(hole.x, piece.x, right);
    const float holeRight = std::clamp(hole.x + hole.width, piece.x, right);
    const float holeTop = std::clamp(hole.y, piece.y, bottom);
    const float holeBottom = std::clamp(hole.y + hole.height, piece.y, bottom);
    if (holeLeft >= holeRight || holeTop >= holeBottom) {
        return {piece};
    }
    const float middle = holeBottom - holeTop;
    return {
        render::Rect{.x = piece.x, .y = piece.y, .width = piece.width, .height = holeTop - piece.y},
        render::Rect{
            .x = piece.x, .y = holeBottom, .width = piece.width, .height = bottom - holeBottom},
        render::Rect{.x = piece.x, .y = holeTop, .width = holeLeft - piece.x, .height = middle},
        render::Rect{.x = holeRight, .y = holeTop, .width = right - holeRight, .height = middle}};
}

void AppendAround(const MenuPicture& picture, std::span<const render::Rect> holes,
                  std::vector<render::SpriteDraw>& out) {
    const std::span<const render::Rect> cut = holes.first(std::min(holes.size(), kMaxHoles));
    std::array<Uncut, (kMaxHoles * (kSidesPerCut - 1)) + 1> stack{};
    std::size_t size = 0;
    stack.at(size++) = {.piece = picture.bounds};
    while (size > 0) {
        const Uncut next = stack.at(--size);
        if (next.hole == cut.size()) {
            AppendPiece(picture, next.piece, out);
            continue;
        }
        for (const render::Rect& side : SidesAround(next.piece, cut[next.hole])) {
            if (side.width > 0.0F && side.height > 0.0F) {
                stack.at(size++) = {.piece = side, .hole = next.hole + 1};
            }
        }
    }
}

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
        AppendSideTiles(picture, out);
        out.push_back({.texture = picture.texture, .bounds = picture.bounds, .tint = picture.tint});
    }
}

render::Rect PieceUv(const render::Rect& bounds, const render::Rect& piece) {
    return {.x = (piece.x - bounds.x) / bounds.width,
            .y = (piece.y - bounds.y) / bounds.height,
            .width = piece.width / bounds.width,
            .height = piece.height / bounds.height};
}

void DrawPicturesAround(std::span<const MenuPicture> pictures, const render::Rect& hole,
                        std::vector<render::SpriteDraw>& out) {
    DrawPicturesAround(pictures, std::span(&hole, 1), out);
}

void DrawPicturesAround(std::span<const MenuPicture> pictures, std::span<const render::Rect> holes,
                        std::vector<render::SpriteDraw>& out) {
    for (const MenuPicture& picture : pictures) {
        AppendSideTiles(picture, out);
        AppendAround(picture, holes, out);
    }
}

void DrawLabel(const MenuText& text, const MenuButtonState& state, std::u16string_view label,
               float x, float y, std::vector<render::SpriteDraw>& out) {
    if (const UiFont* font = text.Font(state.font)) {
        LayoutText(*font, label, {.x = x, .y = y, .tint = state.textColor}, out);
    }
}

}
