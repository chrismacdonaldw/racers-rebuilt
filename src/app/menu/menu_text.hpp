#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "app/ui_text/ui_font.hpp"
#include "assets/vfs.hpp"
#include "render/renderer.hpp"
#include "settings/registry.hpp"

namespace rr::app {

/// The menu fonts and text lines of the language in settings.
class MenuText {
public:
    /// Reloads only when the language changed, and returns whether it did. On failure, logs and
    /// leaves the menus without text.
    bool Apply(render::Renderer& renderer, const assets::Vfs& vfs,
               const settings::Registry& settings);

    [[nodiscard]] std::string_view Language() const { return language_; }

    [[nodiscard]] const UiFont* Font(std::string_view name) const {
        return FindUiFont(fonts_, name);
    }

    /// Empty when the line is missing.
    [[nodiscard]] std::u16string_view Line(std::size_t line) const;

private:
    std::string language_;
    UiFonts fonts_;
    std::vector<std::u16string> lines_;
};

}
