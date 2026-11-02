#include "app/menu/options_look.hpp"

#include <array>
#include <format>
#include <string_view>
#include <utility>
#include <variant>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_page.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "OPTIONS";
constexpr std::string_view kBackdrop = "backdrp";
constexpr std::string_view kBanner = "banner";
constexpr std::string_view kTextButtonStyle = "nubutton";
constexpr std::string_view kBackButtonStyle = "buttonla";
constexpr std::string_view kLabelStyle = "menufont";
constexpr std::string_view kDriverStyle = "driver";
constexpr std::string_view kRailStyle = "rail";
constexpr std::string_view kThumbStyle = "thumb";
constexpr std::string_view kStepDown = "sounddwn";
constexpr std::string_view kStepUp = "soundup";
constexpr std::string_view kPreviousArrow = "arrowlu";
constexpr std::string_view kPreviousArrowFocused = "arrowls";
constexpr std::string_view kNextArrow = "arrowru";
constexpr std::string_view kNextArrowFocused = "arrowrs";
constexpr float kHalf = 0.5F;

struct ButtonPlace {
    std::string_view element;
    OptionsRow row;
    std::size_t textLine;
};

struct LabelPlace {
    std::string_view element;
    OptionsPage page;
    std::size_t textLine;
};

struct WidgetPlace {
    std::string_view element;
    OptionsRow row;
};

constexpr std::array kButtons{
    ButtonPlace{.element = "game", .row = OptionsRow::Game, .textLine = 0x11},
    ButtonPlace{.element = "video", .row = OptionsRow::Video, .textLine = 0x12},
    ButtonPlace{.element = "audio", .row = OptionsRow::Audio, .textLine = 0x13},
    ButtonPlace{.element = "player1", .row = OptionsRow::Player1, .textLine = 0x17},
    ButtonPlace{.element = "player2", .row = OptionsRow::Player2, .textLine = 0x18},
    ButtonPlace{.element = "language", .row = OptionsRow::Language, .textLine = 0x9C},
    ButtonPlace{.element = "credits", .row = OptionsRow::Credits, .textLine = 0x58},
    ButtonPlace{.element = "goback", .row = OptionsRow::Back, .textLine = 0x02},
};

constexpr std::array kLabels{
    LabelPlace{.element = "chmptext", .page = OptionsPage::Game, .textLine = 0x59},
    LabelPlace{.element = "laptext", .page = OptionsPage::Game, .textLine = 0x5A},
    LabelPlace{.element = "dritext", .page = OptionsPage::Video, .textLine = 0x5B},
    LabelPlace{.element = "britext", .page = OptionsPage::Video, .textLine = 0x5C},
    LabelPlace{.element = "mvoltext", .page = OptionsPage::Audio, .textLine = 0x5D},
    LabelPlace{.element = "svoltext", .page = OptionsPage::Audio, .textLine = 0x5E},
    LabelPlace{.element = "monotext", .page = OptionsPage::Audio, .textLine = 0x5F},
};

constexpr std::array kSelectors{
    WidgetPlace{.element = "chmpcont", .row = OptionsRow::Opponents},
    WidgetPlace{.element = "lapcont", .row = OptionsRow::VersusLaps},
    WidgetPlace{.element = "driver", .row = OptionsRow::Driver},
    WidgetPlace{.element = "monocont", .row = OptionsRow::Stereo},
};

constexpr std::array kSliders{
    WidgetPlace{.element = "musicvol", .row = OptionsRow::MusicVolume},
    WidgetPlace{.element = "soundvol", .row = OptionsRow::SoundVolume},
};

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

class LookLoader {
public:
    LookLoader(render::Renderer& renderer, const assets::Vfs& vfs, const MenuFiles& files)
        : files_(&files), images_(renderer, vfs, files) {}

    Result<OptionsLook> Load() {
        Result<void> loaded = LoadImages()
                                  .and_then([&] { return LoadBackdrop(); })
                                  .and_then([&] { return LoadBackButton(); })
                                  .and_then([&] { return LoadTexts(); })
                                  .and_then([&] { return PlaceWidgets(); });
        if (!loaded) {
            return std::unexpected(std::move(loaded.error()));
        }
        look_.textButton = ButtonStates(*files_, kTextButtonStyle);
        PlaceButtons();
        PlaceLabels();
        return std::move(look_);
    }

private:
    Result<void> Upload(std::string_view name, UiImage& out) {
        return images_.Upload(name).transform([&](const UiImage& image) { out = image; });
    }

    Result<void> UploadStyled(std::string_view style, UiImage& out) {
        const assets::ImageStyle* found = files_->FindImageStyle(style);
        if (found == nullptr) {
            return Fail(ErrorCode::NotFound, std::format("there's no image style {}", style));
        }
        return Upload(found->image, out);
    }

    Result<void> LoadImages() {
        return Upload(kPreviousArrow, look_.previous.normal)
            .and_then([&] { return Upload(kPreviousArrowFocused, look_.previous.focused); })
            .and_then([&] { return Upload(kNextArrow, look_.next.normal); })
            .and_then([&] { return Upload(kNextArrowFocused, look_.next.focused); })
            .and_then([&] { return Upload(kStepDown, look_.stepDown); })
            .and_then([&] { return Upload(kStepUp, look_.stepUp); })
            .and_then([&] { return UploadStyled(kRailStyle, look_.rail); })
            .and_then([&] { return UploadStyled(kThumbStyle, look_.thumb); });
    }

    Result<void> LoadBackdrop() {
        UiImage backdrop;
        return UploadStyled(kBackdrop, backdrop).transform([&] {
            look_.backdrop = {.texture = backdrop.texture, .bounds = SizedAt({}, backdrop)};
        });
    }

    Result<void> LoadBackButton() {
        const assets::GameButtonStyle* style = files_->FindGameButtonStyle(kBackButtonStyle);
        if (style == nullptr || !style->button.images) {
            return Fail(ErrorCode::NotFound,
                        std::format("there's no button style {}", kBackButtonStyle));
        }
        look_.backButton = ButtonStates(*files_, kBackButtonStyle);
        for (std::size_t state = 0; state < assets::kControlStates; ++state) {
            UiImage image;
            Result<void> uploaded = Upload(style->button.images->at(state), image);
            if (!uploaded) {
                return uploaded;
            }
            look_.backButton.at(state).image = image.texture;
            look_.backImage = {.width = static_cast<float>(image.widthPx),
                               .height = static_cast<float>(image.heightPx)};
        }
        return {};
    }

    Result<void> LoadTexts() {
        for (const auto& [name, out] :
             {std::pair{kBanner, &look_.banner}, std::pair{kLabelStyle, &look_.label},
              std::pair{kDriverStyle, &look_.driver}}) {
            const assets::TextStyle* style = files_->FindTextStyle(name);
            if (style == nullptr) {
                return Fail(ErrorCode::NotFound, std::format("there's no text style {}", name));
            }
            *out = {.font = style->font,
                    .color = style->color ? ToColor(*style->color) : render::Color{}};
        }
        return {};
    }

    [[nodiscard]] Result<const assets::MenuElement*> Find(std::string_view name) const {
        const assets::MenuElement* element = files_->layout.Find(name);
        if (element == nullptr) {
            return Fail(ErrorCode::NotFound, std::format("{}.MIB has no {}", kScreen, name));
        }
        return element;
    }

    [[nodiscard]] MenuPoint Origin(const assets::MenuElement& element) const {
        return OriginOf(files_->layout, element);
    }

    Result<render::Rect> ArrowAt(MenuPoint selector, std::string_view button,
                                 const UiImage& image) const {
        return Find(button).transform([&](const assets::MenuElement* element) {
            const MenuPoint offset = Origin(*element);
            return SizedAt({.x = selector.x + offset.x, .y = selector.y + offset.y}, image);
        });
    }

    Result<void> PlaceSelector(const WidgetPlace& place) {
        Result<const assets::MenuElement*> element = Find(place.element);
        const auto* selector =
            element ? std::get_if<assets::SelectorElement>(&(*element)->body) : nullptr;
        if (selector == nullptr) {
            return Fail(ErrorCode::InvalidData,
                        std::format("{}.MIB {} isn't a selector", kScreen, place.element));
        }
        const MenuPoint origin = Origin(**element);
        Result<render::Rect> previous =
            ArrowAt(origin, selector->previousButton, look_.previous.normal);
        Result<render::Rect> next = previous.and_then([&](const render::Rect&) {
            return ArrowAt(origin, selector->nextButton, look_.next.normal);
        });
        return next.transform([&](const render::Rect& placed) {
            look_.selectors.push_back({.row = place.row,
                                       .bounds = SpanOf(origin, (*element)->widget.rect),
                                       .previous = *previous,
                                       .next = placed});
        });
    }

    Result<void> PlaceWidgets() {
        for (const WidgetPlace& place : kSelectors) {
            Result<void> placed = PlaceSelector(place);
            if (!placed) {
                return placed;
            }
        }
        for (const WidgetPlace& place : kSliders) {
            Result<const assets::MenuElement*> element = Find(place.element);
            if (!element) {
                return std::unexpected(std::move(element.error()));
            }
            look_.sliders.push_back(
                {.row = place.row, .bounds = SpanOf(Origin(**element), (*element)->widget.rect)});
        }
        return {};
    }

    void PlaceButtons() {
        for (const ButtonPlace& place : kButtons) {
            if (const assets::MenuElement* element = files_->layout.Find(place.element)) {
                const MenuPoint origin = Origin(*element);
                look_.buttons.push_back({.row = place.row,
                                         .textLine = place.textLine,
                                         .bounds = {.x = origin.x, .y = origin.y}});
            }
        }
    }

    void PlaceLabels() {
        if (const assets::MenuElement* banner = files_->layout.Find(kBanner)) {
            look_.bannerTop = Origin(*banner).y;
        }
        for (const LabelPlace& place : kLabels) {
            if (const assets::MenuElement* element = files_->layout.Find(place.element)) {
                const MenuPoint origin = Origin(*element);
                look_.labels.push_back(
                    {.page = place.page, .textLine = place.textLine, .x = origin.x, .y = origin.y});
            }
        }
    }

    const MenuFiles* files_;
    MenuImageCache images_;
    OptionsLook look_;
};

}

Result<OptionsLook> LoadOptionsLook(render::Renderer& renderer, const assets::Vfs& vfs) {
    return ReadMenuFiles(vfs, kScreen).and_then([&](const MenuFiles& files) {
        return LookLoader(renderer, vfs, files).Load();
    });
}

SliderParts LayOutSlider(const render::Rect& bounds, const OptionsLook& look, int step) {
    const auto sizeOf = [](const UiImage& image) {
        return BoxSize{.width = static_cast<float>(image.widthPx),
                       .height = static_cast<float>(image.heightPx)};
    };
    const BoxSize down = sizeOf(look.stepDown);
    const BoxSize up = sizeOf(look.stepUp);
    const BoxSize rail = sizeOf(look.rail);
    const BoxSize thumb = sizeOf(look.thumb);
    const float middle = bounds.y + (bounds.height * kHalf);
    const float trackLeft = bounds.x + down.width;
    const float trackRight = bounds.x + bounds.width - up.width;
    const float lowest = trackLeft + (thumb.width * kHalf);
    const float highest = trackRight - (thumb.width * kHalf);
    const float centre = lowest + ((highest - lowest) * static_cast<float>(step) / kMaxVolumeStep);
    return {.stepDown = {.x = bounds.x, .y = bounds.y, .width = down.width, .height = down.height},
            .stepUp = {.x = trackRight, .y = bounds.y, .width = up.width, .height = up.height},
            .rail = {.x = trackLeft + 1.0F,
                     .y = middle - (rail.height * kHalf),
                     .width = trackRight - trackLeft - 2.0F,
                     .height = rail.height},
            .thumb = {.x = centre - (thumb.width * kHalf),
                      .y = middle - (thumb.height * kHalf),
                      .width = thumb.width,
                      .height = thumb.height}};
}

}
