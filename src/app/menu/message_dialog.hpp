#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/menu/dialog_layout.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_sounds.hpp"
#include "app/menu/menu_text.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/menu_layout.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

enum class DialogButtonStyle : std::uint8_t { Check, Cross, Count };

struct DialogButton {
    std::size_t line = 0;
    DialogButtonStyle style = DialogButtonStyle::Check;
};

struct DialogLook {
    std::array<UiImage, kFrameParts> border;
    render::Color borderTint;
    std::optional<render::Color> fill;
    std::string font;
    render::Color textColor;
    float wrapWidth = 0.0F;
    std::array<ImageButtonLook, static_cast<std::size_t>(DialogButtonStyle::Count)> buttons;
};

/// The original's `DIALOG.MIB` box with its message font and the check and cross button styles.
[[nodiscard]] Result<DialogLook> LoadDialogLook(render::Renderer& renderer, const assets::Vfs& vfs);

/// A message over stacked buttons. It opens on the last button, as the original's confirm boxes
/// open on NO, and closes when one is pressed. Lines are `MENUTEXT.SRF` lines.
class MessageDialog {
public:
    MessageDialog(DialogLook look, std::size_t messageLine, std::vector<DialogButton> buttons);

    [[nodiscard]] bool IsClosed() const { return animation_.Phase() == DialogPhase::Closed; }
    [[nodiscard]] std::size_t Focus() const { return focus_; }

    /// Must run after `text` loads or reloads, since the dialog keeps views into its lines.
    void Measure(const MenuText& text);

    /// Returns the button pressed on this tick.
    std::optional<std::size_t> Tick(const sim::InputFrame& input, const MenuSounds& sounds);
    void Draw(const MenuText& text, std::vector<render::SpriteDraw>& out) const;

private:
    [[nodiscard]] const ImageButtonLook& LookOf(const DialogButton& button) const;
    void MoveFocus(const sim::InputFrame& input, const MenuSounds& sounds);

    DialogLook look_;
    std::size_t messageLine_;
    std::vector<DialogButton> buttons_;
    std::size_t focus_;
    std::vector<std::u16string_view> lines_;
    std::optional<DialogLayout> layout_;
    DialogAnimation animation_;
};

}
