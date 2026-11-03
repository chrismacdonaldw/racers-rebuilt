#include "app/menu/licence_look.hpp"

#include <algorithm>
#include <format>
#include <utility>
#include <variant>

#include "app/menu/menu_sounds.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kBackdrop = "backdrp";
constexpr std::string_view kLicence = "license";
constexpr std::string_view kBanner = "banner";
constexpr std::string_view kNameFrame = "firstbox";
constexpr std::string_view kNameFrameStyle = "roundbox";
constexpr std::string_view kName = "ftext";
constexpr std::string_view kExpression = "swapface";
constexpr std::string_view kExpressionStyle = "nubutton";
constexpr std::string_view kDone = "gonext";
constexpr std::string_view kDoneStyle = "buttonch";
constexpr std::string_view kBack = "goback";
constexpr std::string_view kBackStyle = "buttonca";
constexpr std::array<std::string_view, 3> kTrophyStyles{"gold", "silver", "bronze"};

template <typename Body>
Result<std::pair<const assets::MenuElement*, const Body*>> ElementOf(const MenuFiles& files,
                                                                     std::string_view name) {
    return FindMenuElement(files, kLicenceScreen, name)
        .and_then([&](const assets::MenuElement* element)
                      -> Result<std::pair<const assets::MenuElement*, const Body*>> {
            const auto* body = std::get_if<Body>(&element->body);
            if (body == nullptr) {
                return Fail(ErrorCode::InvalidData,
                            std::format("{}.MIB {} is the wrong kind", kLicenceScreen, name));
            }
            return std::pair{element, body};
        });
}

assets::FrameElement Styled(const assets::FrameElement& frame, const assets::FrameStyle& style) {
    assets::FrameElement styled = frame;
    if (frame.images.front().empty()) {
        styled.images = style.images;
    }
    styled.color = frame.color ? frame.color : style.color;
    styled.fillColor = frame.fillColor ? frame.fillColor : style.fillColor;
    return styled;
}

class LookLoader {
public:
    LookLoader(render::Renderer& renderer, const assets::Vfs& vfs, const MenuFiles& files)
        : files_(&files), images_(renderer, vfs, files) {}

    Result<LicenceLook> Load() && {
        Result<void> loaded =
            Picture(kBackdrop, look_.backdrop, {})
                .and_then([&] { return PlaceAt(kLicence, licenceAt_); })
                .and_then([&] { return Picture(kLicence, look_.licence, licenceAt_); })
                .and_then([&] { return LoadTrophies(); })
                .and_then([&] { return LoadName(); })
                .and_then([&] { return LoadNameFrame(); })
                .and_then([&] { return LoadButtons(); });
        if (!loaded) {
            return std::unexpected(std::move(loaded.error()));
        }
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

    Result<void> Picture(std::string_view style, MenuPicture& out, MenuPoint at) {
        return StyleImage(style).transform([&](const UiImage& image) {
            out = {.texture = image.texture, .bounds = SizedAt(at, image)};
        });
    }

    Result<void> PlaceAt(std::string_view name, MenuPoint& out) {
        return FindMenuElement(*files_, kLicenceScreen, name)
            .transform(
                [&](const assets::MenuElement* found) { out = OriginOf(files_->layout, *found); });
    }

    Result<void> LoadTrophies() {
        Result<void> loaded;
        for (std::size_t trophy = 0; loaded && trophy < kTrophyStyles.size(); ++trophy) {
            loaded = StyleImage(kTrophyStyles.at(trophy)).transform([&](const UiImage& image) {
                look_.trophies.at(trophy) = image;
            });
        }
        for (std::size_t spot = 0; loaded && spot < kLicenceTrophies; ++spot) {
            loaded = PlaceAt(std::format("trophy{}", spot + 1), look_.trophySpots.at(spot));
        }
        return loaded;
    }

    Result<void> LoadName() {
        return ElementOf<assets::TextFieldElement>(*files_, kName)
            .transform([&](const auto& found) {
                const auto& [element, field] = found;
                look_.name.bounds =
                    SpanOf(OriginOf(files_->layout, *element), element->widget.rect);
                look_.name.font = field->font;
                look_.name.maxLength = static_cast<std::size_t>(field->maxLength);
                look_.name.sounds = field->soundIds.value_or(NameSounds{});
                std::copy_n(look_.name.sounds.begin(), look_.name.focusSounds.size(),
                            look_.name.focusSounds.begin());
            });
    }

    Result<void> LoadNameFrame() {
        const assets::FrameStyle* style = files_->FindFrameStyle(kNameFrameStyle);
        if (style == nullptr) {
            return Fail(ErrorCode::NotFound,
                        std::format("there's no frame style {}", kNameFrameStyle));
        }
        return ElementOf<assets::FrameElement>(*files_, kNameFrame)
            .and_then([&](const auto& found) {
                const auto& [element, frame] = found;
                look_.name.frameBounds =
                    SpanOf(OriginOf(files_->layout, *element), element->widget.rect);
                return LoadMenuFrame(images_, Styled(*frame, *style));
            })
            .transform([&](const MenuFrameLook& frame) { look_.name.frame = frame; });
    }

    Result<void> LoadButtons() {
        look_.expressionButton = ButtonStates(*files_, kExpressionStyle);
        return ReadTextStyle(*files_, kBanner)
            .transform([&](MenuTextStyle style) { look_.banner = std::move(style); })
            .and_then([&] { return LoadImageButton(images_, *files_, kDoneStyle); })
            .transform([&](const ImageButtonLook& done) { look_.done = done; })
            .and_then([&] { return LoadImageButton(images_, *files_, kBackStyle); })
            .transform([&](const ImageButtonLook& back) { look_.back = back; })
            .and_then([&] { return PlaceAt(kDone, look_.donePlace); })
            .and_then([&] { return PlaceAt(kBack, look_.backPlace); })
            .and_then([&] { return FindMenuElement(*files_, kLicenceScreen, kExpression); })
            .transform([&](const assets::MenuElement* found) {
                look_.expression = OriginOf(files_->layout, *found);
                const bool hasOwn = found->control && found->control->soundIds;
                look_.expressionSounds = hasOwn ? *found->control->soundIds
                                                : GameButtonSounds(*files_, kExpressionStyle);
            })
            .and_then([&] {
                MenuPoint banner;
                return PlaceAt(kBanner, banner).transform([&] { look_.bannerTop = banner.y; });
            });
    }

    const MenuFiles* files_;
    MenuImageCache images_;
    LicenceLook look_;
    MenuPoint licenceAt_;
};

}

Result<LicenceLook> LoadLicenceLook(render::Renderer& renderer, const assets::Vfs& vfs,
                                    const MenuFiles& files) {
    return LookLoader(renderer, vfs, files).Load();
}

MenuPicture TrophyPicture(const LicenceLook& look, std::size_t circuit, settings::Trophy trophy) {
    const UiImage& image = look.trophies.at(static_cast<std::size_t>(trophy));
    return {.texture = image.texture, .bounds = SizedAt(look.trophySpots.at(circuit), image)};
}

}
