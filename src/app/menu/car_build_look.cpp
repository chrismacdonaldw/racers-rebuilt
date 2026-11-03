#include "app/menu/car_build_look.hpp"

#include <format>
#include <utility>
#include <variant>

#include "app/menu/edit_car_look.hpp"
#include "app/menu/menu_page.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kBackdrop = "backdrp";
constexpr std::string_view kSets = "sets";
constexpr std::string_view kSetBox = "imagebox";
constexpr std::string_view kPieces = "pieces";
constexpr std::string_view kCarousel = "piecesel";
constexpr std::string_view kItemStyle = "partcont";
constexpr std::string_view kScrollStyle = "pieceset";
constexpr std::array<std::string_view, kBuildButtons> kButtons{"procker", "crocker",  "rotbrik",
                                                               "addbrik", "undobrik", "goback"};
constexpr StandSet kGarageSet{.folder = "MENUDATA/GARAGE", .element = "garage"};

template <typename Body>
Result<const Body*> BodyOf(const assets::MenuElement& element) {
    const auto* body = std::get_if<Body>(&element.body);
    if (body == nullptr) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}.MIB {} is the wrong kind", kCarBuildScreen, element.name));
    }
    return body;
}

struct PlacedImages {
    const assets::StateImages* images = nullptr;
    MenuPoint origin;
};

Result<PlacedImages> ButtonImages(const assets::MenuLayout& layout,
                                  const assets::MenuElement& element) {
    if (const auto* hotspot = std::get_if<assets::HotspotElement>(&element.body);
        hotspot != nullptr && hotspot->button) {
        const assets::MenuRect& rect = hotspot->button->widget.rect;
        return PlacedImages{
            .images = &hotspot->button->button.images,
            .origin = {.x = static_cast<float>(rect.left), .y = static_cast<float>(rect.top)}};
    }
    return BodyOf<assets::ButtonElement>(element).transform(
        [&](const assets::ButtonElement* button) {
            return PlacedImages{.images = &button->images, .origin = OriginOf(layout, element)};
        });
}

class LookLoader {
public:
    LookLoader(render::Renderer& renderer, const assets::Vfs& vfs, const MenuFiles& files)
        : renderer_(&renderer), vfs_(&vfs), files_(&files), images_(renderer, vfs, files) {}

    Result<CarBuildLook> Load() && {
        Result<void> loaded = LoadImages()
                                  .and_then([&] { return PlaceSets(); })
                                  .and_then([&] { return PlacePieces(); })
                                  .and_then([&] { return LoadGarage(); });
        for (std::size_t button = 0; loaded && button < kBuildButtons; ++button) {
            loaded = LoadButton(button);
        }
        if (!loaded) {
            return std::unexpected(std::move(loaded.error()));
        }
        const assets::CarouselStyle* scroll = files_->FindCarouselStyle(kScrollStyle);
        look_.scroll = Milliseconds{scroll != nullptr ? scroll->scrollMs.value_or(0) : 0};
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
                .transform([&](const SelectorArrows& arrows) { look_.arrows = arrows; });
        for (std::size_t set = 0; loaded && set < kPartSetCount; ++set) {
            loaded = StyleImage(kSetImageStyles.at(set)).transform([&](const UiImage& image) {
                look_.setImages.at(set) = image;
            });
        }
        const assets::SelectorStyle* items = files_->FindSelectorStyle(kItemStyle);
        if (loaded && (items == nullptr || !items->itemColors)) {
            return Fail(ErrorCode::NotFound,
                        std::format("there's no selector style {} with item colours", kItemStyle));
        }
        return loaded.transform([&] { look_.itemColors = *items->itemColors; });
    }

    Result<render::Rect> Inside(const SelectorPlace& selector, std::string_view name) {
        return FindMenuElement(*files_, kCarBuildScreen, name)
            .transform([&](const assets::MenuElement* element) {
                const assets::MenuRect& rect = element->widget.rect;
                const MenuPoint origin{.x = selector.bounds.x + static_cast<float>(rect.left),
                                       .y = selector.bounds.y + static_cast<float>(rect.top)};
                return SpanOf(origin, rect);
            });
    }

    Result<void> PlaceSets() {
        return PlaceSelector(*files_, kCarBuildScreen, kSets, look_.arrows)
            .and_then([&](const SelectorPlace& sets) {
                look_.sets = sets;
                return Inside(sets, kSetBox);
            })
            .transform([&](const render::Rect& box) { look_.setBox = box; });
    }

    Result<void> PlacePieces() {
        return PlaceSelector(*files_, kCarBuildScreen, kPieces, look_.arrows)
            .and_then([&](const SelectorPlace& pieces) {
                look_.pieces = pieces;
                return Inside(pieces, kCarousel);
            })
            .and_then([&](const render::Rect& bounds) {
                return FindMenuElement(*files_, kCarBuildScreen, kCarousel)
                    .and_then([](const assets::MenuElement* element) {
                        return BodyOf<assets::ModelCarouselElement>(*element);
                    })
                    .and_then([&](const assets::ModelCarouselElement* carousel) {
                        return LayoutCarousel(*carousel, bounds);
                    });
            })
            .transform([&](CarouselLayout layout) { look_.carousel = std::move(layout); });
    }

    Result<void> LoadGarage() {
        return LoadStandFraming(*renderer_, *vfs_, *files_, kCarBuildScreen, kGarageSet)
            .transform([&](const StandFraming& framing) { look_.garage = framing; });
    }

    Result<void> LoadButton(std::size_t index) {
        StateButton& button = look_.buttons.at(index);
        Result<PlacedImages> placed = FindMenuElement(*files_, kCarBuildScreen, kButtons.at(index))
                                          .and_then([&](const assets::MenuElement* found) {
                                              return ButtonImages(files_->layout, *found);
                                          });
        for (std::size_t state = 0; placed && state < assets::kControlStates; ++state) {
            Result<UiImage> image = images_.Upload(placed->images->at(state));
            if (!image) {
                return std::unexpected(std::move(image.error()));
            }
            button.images.at(state) = image->texture;
            if (state == kNormalState) {
                button.bounds = SizedAt(placed->origin, *image);
            }
        }
        return placed.transform([](const PlacedImages&) {});
    }

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    const MenuFiles* files_;
    MenuImageCache images_;
    CarBuildLook look_;
};

}

Result<CarBuildLook> LoadCarBuildLook(render::Renderer& renderer, const assets::Vfs& vfs,
                                      const MenuFiles& files) {
    return LookLoader(renderer, vfs, files).Load();
}

}
