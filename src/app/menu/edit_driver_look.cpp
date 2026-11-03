#include "app/menu/edit_driver_look.hpp"

#include <format>
#include <utility>
#include <variant>

#include "app/menu/menu_sounds.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kBackdrop = "backdrp";
constexpr std::string_view kBanner = "banner";
constexpr std::string_view kItemStyle = "partcont";
constexpr std::string_view kScrollStyle = "picker";
constexpr std::string_view kMixStyle = "nubutton";
constexpr std::string_view kDoneStyle = "buttonch";
constexpr std::string_view kBackStyle = "buttonca";
constexpr std::string_view kMix = "mix";
constexpr std::string_view kDone = "gonext";
constexpr std::string_view kBack = "goback";
constexpr std::array<std::string_view, kDriverSlots> kSelectors{"hatsel", "facesel", "torsosel",
                                                                "legsel"};
constexpr std::array<std::string_view, kDriverSlots> kCarousels{"partbox", "headbox", "partbox",
                                                                "partbox"};

template <typename Body>
Result<const Body*> BodyOf(const MenuFiles& files, std::string_view name) {
    return FindMenuElement(files, kEditDriverScreen, name)
        .and_then([&](const assets::MenuElement* element) -> Result<const Body*> {
            const auto* body = std::get_if<Body>(&element->body);
            if (body == nullptr) {
                return Fail(ErrorCode::InvalidData,
                            std::format("{}.MIB {} is the wrong kind", kEditDriverScreen, name));
            }
            return body;
        });
}

class LookLoader {
public:
    LookLoader(render::Renderer& renderer, const assets::Vfs& vfs, const MenuFiles& files)
        : files_(&files), images_(renderer, vfs, files) {}

    Result<EditDriverLook> Load() && {
        Result<void> loaded = LoadImages()
                                  .and_then([&] { return LoadTexts(); })
                                  .and_then([&] { return LoadBox(); })
                                  .and_then([&] { return Place(); });
        for (std::size_t slot = 0; loaded && slot < kDriverSlots; ++slot) {
            loaded = PlaceRow(slot);
        }
        if (!loaded) {
            return std::unexpected(std::move(loaded.error()));
        }
        look_.mixButton = ButtonStates(*files_, kMixStyle);
        look_.rowSounds = SelectorSounds(*files_, kItemStyle);
        look_.scrollSounds = ScrollSounds(*files_, kScrollStyle);
        const assets::CarouselStyle* scroll = files_->FindCarouselStyle(kScrollStyle);
        look_.scroll = Milliseconds{scroll != nullptr ? scroll->scrollMs.value_or(0) : 0};
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
            .and_then([&] { return LoadImageButton(images_, *files_, kDoneStyle); })
            .transform([&](const ImageButtonLook& done) { look_.done = done; })
            .and_then([&] { return LoadImageButton(images_, *files_, kBackStyle); })
            .transform([&](const ImageButtonLook& back) { look_.back = back; });
    }

    Result<void> LoadTexts() {
        const assets::SelectorStyle* items = files_->FindSelectorStyle(kItemStyle);
        if (items == nullptr || !items->itemColors) {
            return Fail(ErrorCode::NotFound,
                        std::format("there's no selector style {} with item colours", kItemStyle));
        }
        look_.itemColors = *items->itemColors;
        return ReadTextStyle(*files_, kBanner).transform([&](MenuTextStyle style) {
            look_.banner = std::move(style);
        });
    }

    Result<void> LoadBox() {
        return BodyOf<assets::SelectorElement>(*files_, kSelectors.front())
            .and_then([&](const assets::SelectorElement* selector) {
                return BodyOf<assets::FrameElement>(*files_, selector->frame);
            })
            .and_then(
                [&](const assets::FrameElement* frame) { return LoadMenuFrame(images_, *frame); })
            .transform([&](const MenuFrameLook& box) { look_.box = box; });
    }

    Result<void> PlaceAt(std::string_view name, MenuPoint& out,
                         assets::SoundIds* sounds = nullptr) {
        return FindMenuElement(*files_, kEditDriverScreen, name)
            .transform([&](const assets::MenuElement* found) {
                out = OriginOf(files_->layout, *found);
                if (sounds != nullptr) {
                    const bool hasOwn = found->control && found->control->soundIds;
                    *sounds =
                        hasOwn ? *found->control->soundIds : GameButtonSounds(*files_, kMixStyle);
                }
            });
    }

    Result<void> Place() {
        MenuPoint banner;
        return PlaceAt(kMix, look_.mix, &look_.mixSounds)
            .and_then([&] { return PlaceAt(kDone, look_.donePlace); })
            .and_then([&] { return PlaceAt(kBack, look_.backPlace); })
            .and_then([&] { return PlaceAt(kBanner, banner); })
            .transform([&] { look_.bannerTop = banner.y; });
    }

    Result<void> PlaceRow(std::size_t slot) {
        PartRow& row = look_.rows.at(slot);
        return PlaceSelector(*files_, kEditDriverScreen, kSelectors.at(slot), look_.arrows)
            .and_then([&](const SelectorPlace& selector) {
                row.selector = selector;
                return FindMenuElement(*files_, kEditDriverScreen, kCarousels.at(slot));
            })
            .and_then([&](const assets::MenuElement* element) {
                const auto* carousel = std::get_if<assets::ModelCarouselElement>(&element->body);
                if (carousel == nullptr) {
                    return Result<CarouselLayout>(Fail(
                        ErrorCode::InvalidData, std::format("{}.MIB {} isn't a carousel",
                                                            kEditDriverScreen, element->name)));
                }
                const assets::MenuRect& rect = element->widget.rect;
                const MenuPoint origin{.x = row.selector.bounds.x + static_cast<float>(rect.left),
                                       .y = row.selector.bounds.y + static_cast<float>(rect.top)};
                return LayoutCarousel(*carousel, SpanOf(origin, rect));
            })
            .transform([&](CarouselLayout layout) { row.carousel = std::move(layout); });
    }

    const MenuFiles* files_;
    MenuImageCache images_;
    EditDriverLook look_;
};

}

Result<EditDriverLook> LoadEditDriverLook(render::Renderer& renderer, const assets::Vfs& vfs,
                                          const MenuFiles& files) {
    return LookLoader(renderer, vfs, files).Load();
}

}
