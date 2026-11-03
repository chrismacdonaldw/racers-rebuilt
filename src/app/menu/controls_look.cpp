#include "app/menu/controls_look.hpp"

#include <cstddef>
#include <format>
#include <string_view>
#include <utility>

#include "app/menu/menu_files.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "CONTROL";
constexpr std::string_view kBackdrop = "backdrp";
constexpr std::string_view kBanner = "banner";
constexpr std::string_view kBindingStyle = "nubutton";
constexpr std::string_view kFinishStyle = "buttonch";
constexpr std::string_view kLabelStyle = "menufont";
constexpr std::string_view kFinish = "goback";
constexpr std::string_view kDevicePicker = "contcon";
constexpr std::string_view kDeviceImage = "imagebox";

constexpr std::array<std::string_view, kControlRows> kBindingElements{
    "turnleft", "turnrght", "acceler", "brake", "fire", "camera", "map", "slide", "lookback"};
constexpr std::array<std::string_view, kControlRows> kLabelElements{
    "lefttext", "rghttext", "acletext", "brketext", "firetext",
    "camtext",  "maptext",  "pwsltext", "lkbktext"};

class LookLoader {
public:
    LookLoader(render::Renderer& renderer, const assets::Vfs& vfs, const MenuFiles& files)
        : files_(&files), images_(renderer, vfs, files) {}

    Result<ControlsLook> Load() {
        Result<void> loaded = LoadImages()
                                  .and_then([&] { return LoadTexts(); })
                                  .and_then([&] { return PlaceDevicePicker(); })
                                  .and_then([&] { return PlaceRows(); })
                                  .and_then([&] { return PlaceAt(kFinish, look_.finish); })
                                  .and_then([&] { return PlaceBanner(); });
        if (!loaded) {
            return std::unexpected(std::move(loaded.error()));
        }
        look_.bindingButton = ButtonStates(*files_, kBindingStyle);
        return std::move(look_);
    }

private:
    Result<void> Upload(std::string_view name, UiImage& out) {
        return images_.Upload(name).transform([&](const UiImage& image) { out = image; });
    }

    Result<void> LoadImages() {
        const assets::ImageStyle* backdrop = files_->FindImageStyle(kBackdrop);
        if (backdrop == nullptr) {
            return Fail(ErrorCode::NotFound, std::format("there's no image style {}", kBackdrop));
        }
        UiImage backdropImage;
        return Upload(backdrop->image, backdropImage)
            .transform([&] {
                look_.backdrop = {.texture = backdropImage.texture,
                                  .bounds = SizedAt({}, backdropImage)};
            })
            .and_then([&] { return LoadSelectorArrows(images_); })
            .transform([&](const SelectorArrows& arrows) { look_.arrows = arrows; })
            .and_then([&] { return LoadImageButton(images_, *files_, kFinishStyle); })
            .transform([&](const ImageButtonLook& finish) { look_.finishButton = finish; })
            .and_then([&] { return LoadDeviceImages(); });
    }

    Result<void> LoadDeviceImages() {
        for (std::size_t image = 0; image < kDeviceImageNames.size(); ++image) {
            Result<void> loaded = Upload(kDeviceImageNames.at(image), look_.deviceImages.at(image));
            if (!loaded) {
                return loaded;
            }
        }
        return {};
    }

    Result<void> LoadTexts() {
        return ReadTextStyle(*files_, kBanner)
            .transform([&](MenuTextStyle style) { look_.banner = std::move(style); })
            .and_then([&] { return ReadTextStyle(*files_, kLabelStyle); })
            .transform([&](MenuTextStyle style) { look_.label = std::move(style); });
    }

    Result<void> PlaceAt(std::string_view name, MenuPoint& out) const {
        return FindMenuElement(*files_, kScreen, name)
            .transform([&](const assets::MenuElement* element) {
                out = OriginOf(files_->layout, *element);
            });
    }

    Result<void> PlaceDevicePicker() {
        MenuPoint image;
        return PlaceSelector(*files_, kScreen, kDevicePicker, look_.arrows)
            .transform([&](const SelectorPlace& picker) { look_.devicePicker = picker; })
            .and_then([&] { return PlaceAt(kDeviceImage, image); })
            .transform([&] {
                look_.deviceImage = {.x = look_.devicePicker.bounds.x + image.x,
                                     .y = look_.devicePicker.bounds.y + image.y};
            });
    }

    Result<void> PlaceRows() {
        for (std::size_t row = 0; row < kControlRows; ++row) {
            Result<void> placed =
                PlaceAt(kBindingElements.at(row), look_.bindings.at(row)).and_then([&] {
                    return PlaceAt(kLabelElements.at(row), look_.labels.at(row));
                });
            if (!placed) {
                return placed;
            }
        }
        return {};
    }

    Result<void> PlaceBanner() {
        MenuPoint banner;
        return PlaceAt(kBanner, banner).transform([&] { look_.bannerTop = banner.y; });
    }

    const MenuFiles* files_;
    MenuImageCache images_;
    ControlsLook look_;
};

}

Result<ControlsLook> LoadControlsLook(render::Renderer& renderer, const assets::Vfs& vfs) {
    return ReadMenuFiles(vfs, kScreen).and_then([&](const MenuFiles& files) {
        return LookLoader(renderer, vfs, files).Load();
    });
}

}
