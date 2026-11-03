#include "app/menu/racer_pick_look.hpp"

#include <format>
#include <string_view>
#include <utility>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_sounds.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "PICKRCR";
constexpr std::string_view kBackdrop = "backdrp";
constexpr std::string_view kBanner = "banner";
constexpr std::string_view kLabelStyle = "menufont";
constexpr std::string_view kConfigStyle = "nubutton";
constexpr std::string_view kStartStyle = "buttonch";
constexpr std::string_view kBackStyle = "buttonca";
constexpr std::string_view kConfig = "control";
constexpr std::string_view kStart = "gorace";
constexpr std::string_view kBack = "goback";
constexpr std::string_view kPicker = "rcont";
constexpr std::string_view kPickerStyle = "textcont";
constexpr std::string_view kScrollStyle = "picker";

class LookLoader {
public:
    LookLoader(render::Renderer& renderer, const assets::Vfs& vfs, const MenuFiles& files)
        : files_(&files), images_(renderer, vfs, files) {}

    Result<RacerPickLook> Load() {
        Result<void> loaded =
            LoadImages().and_then([&] { return LoadTexts(); }).and_then([&] { return Place(); });
        if (!loaded) {
            return std::unexpected(std::move(loaded.error()));
        }
        look_.configButton = ButtonStates(*files_, kConfigStyle);
        look_.configSounds = GameButtonSounds(*files_, kConfigStyle);
        look_.pickerSounds = SelectorSounds(*files_, kPickerStyle);
        look_.scrollSounds = ScrollSounds(*files_, kScrollStyle);
        if (const assets::MenuElement* banner = files_->layout.Find(kBanner)) {
            look_.bannerTop = OriginOf(files_->layout, *banner).y;
        }
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
            .and_then([&] { return LoadImageButton(images_, *files_, kStartStyle); })
            .transform([&](const ImageButtonLook& start) { look_.startButton = start; })
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
        return FindMenuElement(*files_, kScreen, name)
            .transform(
                [&](const assets::MenuElement* found) { out = OriginOf(files_->layout, *found); });
    }

    Result<void> Place() {
        return PlaceSelector(*files_, kScreen, kPicker, look_.arrows)
            .transform([&](const SelectorPlace& picker) { look_.picker = picker; })
            .and_then([&] { return PlaceAt(kConfig, look_.config); })
            .and_then([&] { return PlaceAt(kStart, look_.start); })
            .and_then([&] { return PlaceAt(kBack, look_.back); });
    }

    const MenuFiles* files_;
    MenuImageCache images_;
    RacerPickLook look_;
};

}

Result<RacerPickLook> LoadRacerPickLook(render::Renderer& renderer, const assets::Vfs& vfs) {
    return ReadMenuFiles(vfs, kScreen).and_then([&](const MenuFiles& files) {
        return LookLoader(renderer, vfs, files).Load();
    });
}

}
