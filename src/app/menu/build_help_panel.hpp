#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/build_help.hpp"
#include "app/menu/menu_text.hpp"
#include "assets/vfs.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// `vfs` and `text` must outlive it.
class BuildHelpPanel {
public:
    BuildHelpPanel(const assets::Vfs& vfs, const MenuText& text) : vfs_(&vfs), text_(&text) {}

    void Tick(std::size_t row, const render::Rect& rowBounds, bool pressed);
    void Draw(std::vector<render::SpriteDraw>& sprites) const;

private:
    const assets::Vfs* vfs_;
    const MenuText* text_;
    std::string language_;
    std::vector<std::u16string> lines_;
    HelpTimer timer_;
    std::optional<HelpBox> box_;
    std::size_t row_ = 0;
};

}
