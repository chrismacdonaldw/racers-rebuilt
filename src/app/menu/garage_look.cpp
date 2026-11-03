#include "app/menu/garage_look.hpp"

#include <format>
#include <string_view>
#include <utility>

#include "app/menu/menu_sounds.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kBackdrop = "backdrp";
constexpr std::string_view kBanner = "banner";
constexpr std::string_view kLabelStyle = "menufont";
constexpr std::string_view kButtonStyle = "nubutton";
constexpr std::string_view kBackStyle = "buttonla";
constexpr std::string_view kBack = "goback";
constexpr std::string_view kPicker = "rcont";
constexpr std::string_view kScrollStyle = "picker";
constexpr std::array<std::string_view, kGarageButtons> kButtonNames{
    "newracer", "editracr", "copyracr", "delracer", "testtrck", "editdrvr", "editlice", "editcar"};

class LookLoader {
public:
    LookLoader(render::Renderer& renderer, const assets::Vfs& vfs, const MenuFiles& files)
        : files_(&files), images_(renderer, vfs, files) {}

    Result<GarageLook> Load() {
        Result<void> loaded =
            LoadImages().and_then([&] { return LoadTexts(); }).and_then([&] { return Place(); });
        if (!loaded) {
            return std::unexpected(std::move(loaded.error()));
        }
        look_.button = ButtonStates(*files_, kButtonStyle);
        look_.buttonSounds = GameButtonSounds(*files_, kButtonStyle);
        look_.scrollSounds = ScrollSounds(*files_, kScrollStyle);
        return std::move(look_);
    }

private:
    Result<void> LoadImages() {
        const assets::ImageStyle* backdrop = files_->FindImageStyle(kBackdrop);
        if (backdrop == nullptr) {
            return Fail(ErrorCode::NotFound, std::format("there's no image style {}", kBackdrop));
        }
        return images_.Upload(backdrop->image)
            .transform([&](const UiImage& image) {
                look_.backdrop = {.texture = image.texture, .bounds = SizedAt({}, image)};
            })
            .and_then([&] { return LoadSelectorArrows(images_); })
            .transform([&](const SelectorArrows& arrows) { look_.arrows = arrows; })
            .and_then([&] { return LoadImageButton(images_, *files_, kBackStyle); })
            .transform([&](const ImageButtonLook& back) { look_.backButton = back; });
    }

    Result<void> LoadTexts() {
        return ReadTextStyle(*files_, kBanner)
            .transform([&](MenuTextStyle style) { look_.banner = std::move(style); })
            .and_then([&] { return ReadTextStyle(*files_, kLabelStyle); })
            .transform([&](MenuTextStyle style) { look_.label = std::move(style); });
    }

    Result<void> PlaceAt(std::string_view name, MenuPoint& out) {
        return FindMenuElement(*files_, kGarageScreen, name)
            .transform(
                [&](const assets::MenuElement* found) { out = OriginOf(files_->layout, *found); });
    }

    Result<void> Place() {
        Result<void> placed =
            PlaceSelector(*files_, kGarageScreen, kPicker, look_.arrows)
                .transform([&](const SelectorPlace& picker) { look_.picker = picker; })
                .and_then([&] { return PlaceAt(kBack, look_.back); });
        for (std::size_t button = 0; placed && button < kGarageButtons; ++button) {
            placed = PlaceAt(kButtonNames.at(button), look_.buttons.at(button));
        }
        MenuPoint banner;
        return placed.and_then([&] { return PlaceAt(kBanner, banner); }).transform([&] {
            look_.bannerTop = banner.y;
        });
    }

    const MenuFiles* files_;
    MenuImageCache images_;
    GarageLook look_;
};

}

Result<GarageLook> LoadGarageLook(render::Renderer& renderer, const assets::Vfs& vfs,
                                  const MenuFiles& files) {
    return LookLoader(renderer, vfs, files).Load();
}

}
