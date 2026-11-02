#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/menu/dialog_layout.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_text.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/menu_layout.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

struct DialogLook {
    std::array<UiImage, kFrameParts> border;
    render::Color borderTint;
    std::optional<render::Color> fill;
    std::string font;
    render::Color textColor;
    float wrapWidth = 0.0F;
    BoxSize buttonImage;
    std::array<MenuButtonState, assets::kControlStates> button;
};

/// The original's `DIALOG.MIB` box with its message font and the check-mark button style.
[[nodiscard]] Result<DialogLook> LoadDialogLook(render::Renderer& renderer, const assets::Vfs& vfs);

/// A message and one button that closes it on Confirm. Lines are `MENUTEXT.SRF` lines.
class MessageDialog {
public:
    MessageDialog(DialogLook look, std::size_t messageLine, std::size_t buttonLine)
        : look_(std::move(look)), messageLine_(messageLine), buttonLine_(buttonLine) {}

    [[nodiscard]] bool IsClosed() const { return animation_.Phase() == DialogPhase::Closed; }

    /// Must run after `text` loads or reloads, since the dialog keeps views into its lines.
    void Measure(const MenuText& text);
    void Tick(const sim::InputFrame& input);
    void Draw(const MenuText& text, std::vector<render::SpriteDraw>& out) const;

private:
    DialogLook look_;
    std::size_t messageLine_;
    std::size_t buttonLine_;
    std::vector<std::u16string_view> lines_;
    std::optional<DialogContent> content_;
    DialogAnimation animation_;
};

}
