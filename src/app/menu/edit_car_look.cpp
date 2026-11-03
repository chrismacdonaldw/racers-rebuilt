#include "app/menu/edit_car_look.hpp"

#include <format>
#include <iterator>
#include <utility>
#include <variant>
#include <vector>

#include "app/menu/menu_sounds.hpp"
#include "assets/game_path.hpp"
#include "assets/image_list.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kBuilderImages = "BUILDER.IDB";
constexpr std::string_view kBackdrop = "backdrp";
constexpr std::string_view kBanner = "banner";
constexpr std::string_view kPicker = "selector";
constexpr std::string_view kPickerStyle = "imagecon";
constexpr std::string_view kScrollStyle = "picker";
constexpr std::string_view kSetBox = "imagebox";
constexpr std::string_view kTextStyle = "nubutton";
constexpr std::string_view kDone = "gonext";
constexpr std::string_view kDoneStyle = "buttonch";
constexpr std::string_view kBack = "goback";
constexpr std::string_view kBackStyle = "buttonca";
constexpr std::array<std::string_view, kCarTextButtons> kTextButtons{"build", "ripapart", "qbuild"};
constexpr std::array<std::string_view, kPartSetCount> kSetStyles{
    "bricks",  "race",     "space",  "castle", "pirate", "islander",
    "magical", "adventur", "jungle", "alien",  "rr",     "vv"};

class LookLoader {
public:
    LookLoader(render::Renderer& renderer, const assets::Vfs& vfs, const MenuFiles& files)
        : files_(&files), images_(renderer, vfs, files) {}

    Result<EditCarLook> Load() && {
        Result<void> loaded = LoadImages().and_then([&] { return LoadPicker(); }).and_then([&] {
            return LoadButtons();
        });
        if (!loaded) {
            return std::unexpected(std::move(loaded.error()));
        }
        look_.pickerSounds = SelectorSounds(*files_, kPickerStyle);
        look_.scrollSounds = ScrollSounds(*files_, kScrollStyle);
        look_.textButton = ButtonStates(*files_, kTextStyle);
        return std::move(look_);
    }

private:
    Result<UiImage> StyleImage(std::string_view style) {
        const assets::ImageStyle* image = files_->FindImageStyle(style);
        if (image == nullptr) {
            return Fail(ErrorCode::NotFound, std::format("there's no image style {}", style));
        }
        return images_.Upload(image->image);
    }

    Result<void> LoadImages() {
        Result<void> loaded =
            StyleImage(kBackdrop)
                .transform([&](const UiImage& image) {
                    look_.backdrop = {.texture = image.texture, .bounds = SizedAt({}, image)};
                })
                .and_then([&] { return LoadSelectorArrows(images_); })
                .transform([&](const SelectorArrows& arrows) { look_.arrows = arrows; })
                .and_then([&] { return LoadImageButton(images_, *files_, kDoneStyle); })
                .transform([&](const ImageButtonLook& done) { look_.done = done; })
                .and_then([&] { return LoadImageButton(images_, *files_, kBackStyle); })
                .transform([&](const ImageButtonLook& back) { look_.back = back; });
        for (std::size_t set = 0; loaded && set < kPartSetCount; ++set) {
            loaded = StyleImage(kSetStyles.at(set)).transform([&](const UiImage& image) {
                look_.sets.at(set) = image;
            });
        }
        return loaded;
    }

    Result<void> LoadPicker() {
        return PlaceSelector(*files_, kEditCarScreen, kPicker, look_.arrows)
            .and_then([&](const SelectorPlace& picker) {
                look_.picker = picker;
                return FindMenuElement(*files_, kEditCarScreen, kPicker);
            })
            .and_then(
                [&](const assets::MenuElement* element) -> Result<const assets::MenuElement*> {
                    const auto* selector = std::get_if<assets::SelectorElement>(&element->body);
                    if (selector == nullptr) {
                        return Fail(
                            ErrorCode::InvalidData,
                            std::format("{}.MIB {} isn't a selector", kEditCarScreen, kPicker));
                    }
                    return FindMenuElement(*files_, kEditCarScreen, selector->frame);
                })
            .and_then([&](const assets::MenuElement* element) -> Result<MenuFrameLook> {
                const auto* frame = std::get_if<assets::FrameElement>(&element->body);
                if (frame == nullptr) {
                    return Fail(ErrorCode::InvalidData, std::format("{}.MIB {} isn't a frame",
                                                                    kEditCarScreen, element->name));
                }
                return LoadMenuFrame(images_, *frame);
            })
            .transform([&](const MenuFrameLook& box) { look_.box = box; })
            .and_then([&] { return FindMenuElement(*files_, kEditCarScreen, kSetBox); })
            .transform([&](const assets::MenuElement* element) {
                const assets::MenuRect& rect = element->widget.rect;
                const MenuPoint origin{.x = look_.picker.bounds.x + static_cast<float>(rect.left),
                                       .y = look_.picker.bounds.y + static_cast<float>(rect.top)};
                look_.setBox = SpanOf(origin, rect);
            });
    }

    Result<void> PlaceAt(std::string_view name, MenuPoint& out,
                         assets::SoundIds* sounds = nullptr) {
        return FindMenuElement(*files_, kEditCarScreen, name)
            .transform([&](const assets::MenuElement* found) {
                out = OriginOf(files_->layout, *found);
                if (sounds != nullptr) {
                    const bool hasOwn = found->control && found->control->soundIds;
                    *sounds =
                        hasOwn ? *found->control->soundIds : GameButtonSounds(*files_, kTextStyle);
                }
            });
    }

    Result<void> LoadButtons() {
        Result<void> loaded =
            ReadTextStyle(*files_, kBanner)
                .transform([&](MenuTextStyle style) { look_.banner = std::move(style); })
                .and_then([&] { return PlaceAt(kDone, look_.donePlace); })
                .and_then([&] { return PlaceAt(kBack, look_.backPlace); });
        for (std::size_t button = 0; loaded && button < kCarTextButtons; ++button) {
            loaded = PlaceAt(kTextButtons.at(button), look_.textPlaces.at(button),
                             &look_.textSounds.at(button));
        }
        MenuPoint banner;
        return loaded.and_then([&] { return PlaceAt(kBanner, banner); }).transform([&] {
            look_.bannerTop = banner.y;
        });
    }

    const MenuFiles* files_;
    MenuImageCache images_;
    EditCarLook look_;
};

}

Result<void> AddBuilderImages(const assets::Vfs& vfs, MenuFiles& files) {
    return vfs.Read(assets::JoinGamePath(kMenuFolder, kBuilderImages))
        .and_then([](const assets::FileData& data) { return assets::ParseImageList(data.Bytes()); })
        .transform([&](std::vector<assets::ImageListEntry> entries) {
            files.images.insert(files.images.end(), std::make_move_iterator(entries.begin()),
                                std::make_move_iterator(entries.end()));
        });
}

Result<EditCarLook> LoadEditCarLook(render::Renderer& renderer, const assets::Vfs& vfs,
                                    const MenuFiles& files) {
    return LookLoader(renderer, vfs, files).Load();
}

}
