#include "app/menu/main_menu_scene.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

#include "app/game_settings.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_focus.hpp"
#include "app/ui_text/text_layout.hpp"
#include "assets/game_path.hpp"
#include "assets/install.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "MAINMENU";
constexpr std::string_view kButtonStyle = "nubutton";
constexpr std::string_view kFontList = "GFONTS.FDB";
constexpr std::size_t kFirstTextColor = 7;
constexpr std::size_t kNormalState = 2;
constexpr std::size_t kFocusedState = 4;
constexpr float kChannelMax = 255.0F;

struct ButtonLine {
    std::string_view button;
    std::size_t line = 0;
};

constexpr std::array kButtonLines{
    ButtonLine{.button = "garage", .line = 37}, ButtonLine{.button = "circuit", .line = 33},
    ButtonLine{.button = "single", .line = 34}, ButtonLine{.button = "vs", .line = 35},
    ButtonLine{.button = "time", .line = 36},   ButtonLine{.button = "options", .line = 38},
    ButtonLine{.button = "quit", .line = 39},
};

constexpr std::array kFocusMoves{
    std::pair{sim::MenuAction::Up, FocusMove::Up},
    std::pair{sim::MenuAction::Down, FocusMove::Down},
    std::pair{sim::MenuAction::Left, FocusMove::Left},
    std::pair{sim::MenuAction::Right, FocusMove::Right},
};

struct Point {
    float x = 0.0F;
    float y = 0.0F;
};

render::Color ToColor(const assets::MenuColor& color) {
    return {.r = static_cast<float>(color.red) / kChannelMax,
            .g = static_cast<float>(color.green) / kChannelMax,
            .b = static_cast<float>(color.blue) / kChannelMax,
            .a = static_cast<float>(color.alpha) / kChannelMax};
}

const assets::MenuElement* FindById(const assets::MenuLayout& layout, std::int32_t id) {
    const auto element = std::ranges::find(layout.elements, id, &assets::MenuElement::id);
    return element == layout.elements.end() ? nullptr : &*element;
}

Point OriginOf(const assets::MenuLayout& layout, const assets::MenuElement& element) {
    Point origin;
    const assets::MenuElement* at = &element;
    for (std::size_t depth = 0; at != nullptr && depth <= layout.elements.size(); ++depth) {
        origin.x += static_cast<float>(at->widget.rect.left);
        origin.y += static_cast<float>(at->widget.rect.top);
        at = FindById(layout, at->parentId);
    }
    return origin;
}

render::Rect BoundsOf(const assets::MenuRect& rect, Point origin, const MenuImage& image) {
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

class MenuBuilder {
public:
    MenuBuilder(render::Renderer& renderer, const assets::Vfs& vfs, const MenuFiles& files)
        : renderer_(&renderer), vfs_(&vfs), files_(&files) {}

    Result<void> Add(const assets::MenuElement& element) {
        const Point origin = OriginOf(files_->layout, element);
        if (const auto* image = std::get_if<assets::ImageElement>(&element.body)) {
            return AddPicture(element, *image, origin);
        }
        const auto* button = std::get_if<assets::GameButtonElement>(&element.body);
        const auto line =
            std::ranges::find(kButtonLines, std::string_view(element.name), &ButtonLine::button);
        if (button == nullptr || line == kButtonLines.end()) {
            return {};
        }
        return AddButton(element, *button, origin, line->line);
    }

    [[nodiscard]] std::vector<MenuPicture>& Pictures() { return pictures_; }
    [[nodiscard]] std::vector<MenuButton>& Buttons() { return buttons_; }

private:
    Result<MenuImage> Upload(std::string_view name) {
        const std::string key = assets::ToUpperAscii(name);
        if (const auto image = images_.find(key); image != images_.end()) {
            return image->second;
        }
        const assets::ImageListEntry* entry = files_->FindImage(name);
        if (entry == nullptr) {
            return Fail(ErrorCode::NotFound,
                        std::format("menu image {} isn't in any image list", name));
        }
        return LoadMenuImage(*renderer_, *vfs_, *entry).transform([&](const MenuImage& image) {
            images_.emplace(key, image);
            return image;
        });
    }

    Result<void> AddPicture(const assets::MenuElement& element, const assets::ImageElement& image,
                            Point origin) {
        const assets::ImageStyle* style = files_->FindImageStyle(element.name);
        const std::string_view name =
            !image.image.empty() || style == nullptr ? std::string_view(image.image) : style->image;
        std::optional<assets::MenuColor> color = image.color;
        if (!color && style != nullptr) {
            color = style->color;
        }
        return Upload(name).transform([&](const MenuImage& uploaded) {
            pictures_.push_back({.texture = uploaded.texture,
                                 .bounds = BoundsOf(element.widget.rect, origin, uploaded),
                                 .tint = color ? ToColor(*color) : render::Color{}});
        });
    }

    Result<void> AddButton(const assets::MenuElement& element, const assets::GameButtonElement& own,
                           Point origin, std::size_t textLine) {
        const assets::GameButtonStyle* found = files_->FindGameButtonStyle(kButtonStyle);
        const assets::GameButtonElement style =
            found != nullptr ? found->button : assets::GameButtonElement{};
        const auto& colors = own.colors ? own.colors : style.colors;
        MenuButton button{.name = element.name, .textLine = textLine};
        for (std::size_t state = 0; state < assets::kControlStates; ++state) {
            Result<MenuImage> image = Upload(PickName(own.images, style.images, state));
            if (!image) {
                return std::unexpected(std::move(image.error()));
            }
            if (state == kNormalState) {
                button.bounds = BoundsOf(element.widget.rect, origin, *image);
            }
            button.states.at(state) = {
                .image = image->texture,
                .font = std::string(PickName(own.fonts, style.fonts, state)),
                .textColor =
                    colors ? ToColor(colors->at(kFirstTextColor + state)) : render::Color{}};
        }
        buttons_.push_back(std::move(button));
        return {};
    }

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    const MenuFiles* files_;
    std::map<std::string, MenuImage, std::less<>> images_;
    std::vector<MenuPicture> pictures_;
    std::vector<MenuButton> buttons_;
};

}

Result<std::unique_ptr<MainMenuScene>> MainMenuScene::Create(render::Renderer& renderer,
                                                             const assets::Vfs& vfs) {
    Result<MenuFiles> files = ReadMenuFiles(vfs, kScreen);
    if (!files) {
        return std::unexpected(std::move(files.error()));
    }
    MenuBuilder builder(renderer, vfs, *files);
    for (const assets::MenuElement& element : files->layout.elements) {
        Result<void> added = builder.Add(element);
        if (!added) {
            return std::unexpected(std::move(added.error()));
        }
    }
    return std::unique_ptr<MainMenuScene>(new MainMenuScene(
        renderer, vfs, std::move(builder.Pictures()), std::move(builder.Buttons())));
}

void MainMenuScene::ApplySettings(const settings::Registry& settings) {
    std::string language(settings.Get(kGameLanguage));
    if (language.empty()) {
        language = assets::kDefaultLanguage;
    }
    if (language == language_) {
        return;
    }
    language_ = std::move(language);
    Result<UiFonts> fonts =
        LoadUiFonts(*renderer_, *vfs_, MenuLanguageFolder(language_), kFontList);
    Result<std::vector<std::u16string>> text = ReadMenuText(*vfs_, language_);
    if (!fonts || !text) {
        log::Error("main menu has no text: {}", (fonts ? text.error() : fonts.error()).message);
    }
    fonts_ = std::move(fonts).value_or(UiFonts{});
    text_ = std::move(text).value_or(std::vector<std::u16string>{});
}

void MainMenuScene::Tick(const sim::InputFrame& input) {
    if (buttons_.empty()) {
        return;
    }
    for (const auto& [action, move] : kFocusMoves) {
        if (input.WasPressed(action)) {
            focus_ = NextFocus(buttonBounds_, focus_, move);
        }
    }
    if (input.WasPressed(sim::MenuAction::Confirm)) {
        log::Info("main menu: {} isn't built yet", buttons_[focus_].name);
    }
}

void MainMenuScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = focus_;
}

void MainMenuScene::Render([[maybe_unused]] const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    for (const MenuPicture& picture : pictures_) {
        sprites_.push_back(
            {.texture = picture.texture, .bounds = picture.bounds, .tint = picture.tint});
    }
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        DrawButton(buttons_[i], i == focus_ ? kFocusedState : kNormalState);
    }
    Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

MainMenuScene::MainMenuScene(render::Renderer& renderer, const assets::Vfs& vfs,
                             std::vector<MenuPicture> pictures, std::vector<MenuButton> buttons)
    : renderer_(&renderer),
      vfs_(&vfs),
      pictures_(std::move(pictures)),
      buttons_(std::move(buttons)) {
    for (const MenuButton& button : buttons_) {
        buttonBounds_.push_back(button.bounds);
    }
}

void MainMenuScene::DrawButton(const MenuButton& button, std::size_t state) {
    const MenuButtonState& look = button.states.at(state);
    sprites_.push_back({.texture = look.image, .bounds = button.bounds});
    const UiFont* font = FindUiFont(fonts_, look.font);
    if (font == nullptr || button.textLine >= text_.size()) {
        return;
    }
    LayoutText(*font, text_[button.textLine],
               {.x = button.bounds.x, .y = button.bounds.y, .tint = look.textColor}, sprites_);
}

}
