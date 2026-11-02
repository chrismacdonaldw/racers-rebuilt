#include "app/menu/options_look.hpp"

#include <array>
#include <format>
#include <string_view>
#include <utility>

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
        return LoadSelectorArrows(images_)
            .transform([&](const SelectorArrows& arrows) { look_.arrows = arrows; })
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
        return LoadImageButton(images_, *files_, kBackButtonStyle)
            .transform([&](const ImageButtonLook& look) { look_.backButton = look; });
    }

    Result<void> LoadTexts() {
        for (const auto& [name, out] :
             {std::pair{kBanner, &look_.banner}, std::pair{kLabelStyle, &look_.label},
              std::pair{kDriverStyle, &look_.driver}}) {
            Result<MenuTextStyle> style = ReadTextStyle(*files_, name);
            if (!style) {
                return std::unexpected(std::move(style.error()));
            }
            *out = std::move(*style);
        }
        return {};
    }

    [[nodiscard]] Result<const assets::MenuElement*> Find(std::string_view name) const {
        return FindMenuElement(*files_, kScreen, name);
    }

    [[nodiscard]] MenuPoint Origin(const assets::MenuElement& element) const {
        return OriginOf(files_->layout, element);
    }

    Result<void> PlaceWidgets() {
        for (const WidgetPlace& place : kSelectors) {
            Result<SelectorPlace> placed =
                PlaceSelector(*files_, kScreen, place.element, look_.arrows);
            if (!placed) {
                return std::unexpected(std::move(placed.error()));
            }
            look_.selectors.push_back({.row = place.row, .place = *placed});
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
