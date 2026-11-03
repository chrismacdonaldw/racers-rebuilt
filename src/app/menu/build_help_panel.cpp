#include "app/menu/build_help_panel.hpp"

#include <array>
#include <string_view>
#include <utility>

#include "app/frame_loop.hpp"
#include "app/menu/car_build_menu.hpp"
#include "app/menu/menu_files.hpp"
#include "core/log.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kHelpFile = "CARBUILD.SRF";
constexpr std::array<std::optional<std::size_t>, kBuildRows> kRowHelp{0, 1, 2, 6, 3, 4, 5, {}};

std::vector<std::u16string> ReadHelp(const assets::Vfs& vfs, std::string_view language) {
    Result<std::vector<std::u16string>> lines = ReadMenuText(vfs, language, kHelpFile);
    if (!lines) {
        log::Warn("the car builder has no help text: {}", lines.error().message);
    }
    return std::move(lines).value_or(std::vector<std::u16string>{});
}

}

void BuildHelpPanel::Tick(std::size_t row, const render::Rect& rowBounds, bool pressed) {
    if (const std::string_view language = text_->Language();
        !language.empty() && language != language_) {
        language_ = language;
        lines_ = ReadHelp(*vfs_, language);
        box_.reset();
    }
    const std::optional<std::size_t> line = kRowHelp.at(row);
    const std::u16string_view text =
        line && *line < lines_.size() ? std::u16string_view(lines_[*line]) : std::u16string_view{};
    timer_.Tick(kSimTick, row, pressed, !text.empty());
    if (!timer_.IsShown()) {
        box_.reset();
        return;
    }
    if (box_ && row_ == row) {
        return;
    }
    const UiFont* font = text_->Font(kBuildHelpFont);
    if (font == nullptr || text.empty()) {
        box_.reset();
        return;
    }
    box_ = LayoutHelp(*font, text, rowBounds,
                      {.width = render::kUiWidth, .height = render::kUiHeight});
    row_ = row;
}

void BuildHelpPanel::Draw(std::vector<render::SpriteDraw>& sprites) const {
    const UiFont* font = text_->Font(kBuildHelpFont);
    if (box_ && font != nullptr) {
        DrawHelp(*font, *box_, sprites);
    }
}

}
