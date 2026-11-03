#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_text.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/menu_layout.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

inline constexpr std::size_t kNormalState = 2;
inline constexpr std::size_t kFocusedState = 4;

struct ButtonLine {
    std::string_view button;
    std::size_t line = 0;
};

struct MenuPage {
    std::vector<MenuPicture> pictures;
    std::vector<MenuButton> buttons;
};

/// Uploads each image once. `renderer`, `vfs` and `files` must outlive it.
class MenuImageCache {
public:
    MenuImageCache(render::Renderer& renderer, const assets::Vfs& vfs, const MenuFiles& files)
        : renderer_(&renderer), vfs_(&vfs), files_(&files) {}

    [[nodiscard]] Result<UiImage> Upload(std::string_view name);

private:
    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    const MenuFiles* files_;
    std::map<std::string, UiImage, std::less<>> images_;
};

[[nodiscard]] render::Color ToColor(const assets::MenuColor& color);

struct MenuPoint {
    float x = 0.0F;
    float y = 0.0F;
};

/// The element's top left on the UI canvas, through its parents.
[[nodiscard]] MenuPoint OriginOf(const assets::MenuLayout& layout,
                                 const assets::MenuElement& element);

/// A picture per image element, and a text-styled button per game button in `labels`.
[[nodiscard]] Result<MenuPage> BuildMenuPage(render::Renderer& renderer, const assets::Vfs& vfs,
                                             const MenuFiles& files,
                                             std::span<const ButtonLine> labels);

/// The game button style's states, without their images. A missing style leaves them blank.
[[nodiscard]] std::array<MenuButtonState, assets::kControlStates> ButtonStates(
    const MenuFiles& files, std::string_view style);

[[nodiscard]] std::array<MenuButtonState, assets::kControlStates> TextButtonStates(
    const MenuFiles& files);

void DrawPictures(std::span<const MenuPicture> pictures, std::vector<render::SpriteDraw>& out);

/// The texture area `piece` shows of a picture drawn over `bounds`.
[[nodiscard]] render::Rect PieceUv(const render::Rect& bounds, const render::Rect& piece);

/// Leaves `hole` uncovered, so a scene drawn there shows through the sprites.
void DrawPicturesAround(std::span<const MenuPicture> pictures, const render::Rect& hole,
                        std::vector<render::SpriteDraw>& out);

void DrawPicturesAround(std::span<const MenuPicture> pictures, std::span<const render::Rect> holes,
                        std::vector<render::SpriteDraw>& out);

/// Draws nothing when the state's font is missing.
void DrawLabel(const MenuText& text, const MenuButtonState& state, std::u16string_view label,
               float x, float y, std::vector<render::SpriteDraw>& out);

}
