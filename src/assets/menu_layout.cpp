#include "assets/menu_layout.hpp"

#include <algorithm>
#include <format>
#include <limits>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {

static Result<void> ReadValue(TokenReader& reader, MenuRect& rect) {
    return ReadMembers<&MenuRect::left, &MenuRect::top, &MenuRect::right, &MenuRect::bottom>(reader,
                                                                                             rect);
}

static Result<void> ReadValue(TokenReader& reader, MenuColor& color) {
    return ReadMembers<&MenuColor::alpha, &MenuColor::red, &MenuColor::green, &MenuColor::blue>(
        reader, color);
}

static Result<void> ReadValue(TokenReader& reader, MenuWidget& widget);

namespace {

constexpr FieldId kCapacityField = 0x27;
constexpr FieldId kWidgetField = 0x36;
constexpr std::size_t kNameLength = 8;
constexpr std::int32_t kFirstElementId = 2;
constexpr std::int32_t kMaxElementId = std::numeric_limits<std::int16_t>::max();
constexpr std::size_t kMaxImageSets = 3;

std::string_view NameKey(std::string_view name) {
    return name.substr(0, std::min(name.size(), kNameLength));
}

constexpr std::array kWidgetFields{
    Field<&MenuWidget::rect>(0x2F),   Field<&MenuWidget::id>(0x30),
    Field<&MenuWidget::parent>(0x31), Field<&MenuWidget::attachesToParent>(0x32),
    Field<&MenuWidget::color>(0x2A),
};

constexpr std::array kControlFields{
    Field<&MenuControl::startsEnabled>(0x33), Field<&MenuControl::attachesToParent>(0x32),
    Field<&MenuControl::name>(0x31),          Field<&MenuControl::value34>(0x34),
    Field<&MenuControl::helpStringId>(0x2C),  Field<&MenuControl::stateColors>(0x2A),
    Field<&MenuControl::soundIds>(0x2B),
};

template <typename Target>
const FieldRule<Target>* FindRule(std::span<const FieldRule<Target>> rules, FieldId id) {
    const auto rule = std::ranges::find(rules, id, &FieldRule<Target>::id);
    return rule == rules.end() ? nullptr : &*rule;
}

template <typename Body>
Result<void> ReadBodyField(TokenReader& reader, std::span<const FieldRule<Body>> rules,
                           MenuWidget& widget, MenuControl* control, Body& body) {
    Result<Token> field = reader.Expect(TokenKind::Field);
    if (!field) {
        return std::unexpected(std::move(field.error()));
    }
    if (const FieldRule<Body>* rule = FindRule(rules, field->field)) {
        return rule->read(reader, body);
    }
    if (field->field == kWidgetField) {
        return ReadValue(reader, widget);
    }
    const FieldRule<MenuControl>* rule =
        control == nullptr ? nullptr : FindRule<MenuControl>(kControlFields, field->field);
    if (rule == nullptr) {
        return Fail(ErrorCode::InvalidData, std::format("unknown menu field 0x{:02X} at offset {}",
                                                        field->field, field->offset));
    }
    return rule->read(reader, *control);
}

template <typename Body>
Result<void> ReadBody(TokenReader& reader, std::span<const FieldRule<Body>> rules,
                      MenuWidget& widget, MenuControl* control, Body& body) {
    Result<Token> open = reader.Expect(TokenKind::OpenBrace);
    if (!open) {
        return std::unexpected(std::move(open.error()));
    }
    while (true) {
        Result<Token> next = reader.Peek();
        if (!next) {
            return std::unexpected(std::move(next.error()));
        }
        if (next->kind == TokenKind::CloseBrace) {
            return reader.Next().transform([](const Token&) {});
        }
        Result<void> read = ReadBodyField(reader, rules, widget, control, body);
        if (!read) {
            return read;
        }
    }
}

constexpr std::array kImageFields{
    Field<&ImageElement::image>(0x28),
    Field<&ImageElement::color>(0x2A),
};

constexpr std::array kTextLabelFields{
    Field<&TextLabelElement::font, &TextLabelElement::fontColor>(0x29),
    Field<&TextLabelElement::color>(0x2A),
    Field<&TextLabelElement::wrapWidth>(0x33),
};

constexpr std::array kFrameFields{
    Field<&FrameElement::images>(0x28),
    Field<&FrameElement::color, &FrameElement::fillColor>(0x2A),
    Field<&FrameElement::hasFillColor>(0x33),
};

constexpr std::array kButtonFields{
    Field<&ButtonElement::images>(0x28),
};

Result<void> ReadImageSet(TokenReader& reader, MultiStateElement& element) {
    if (element.imageSets.size() == kMaxImageSets) {
        return Fail(ErrorCode::InvalidData,
                    std::format("multi-state button has over {} image sets", kMaxImageSets));
    }
    return ReadValue(reader, element.imageSets.emplace_back());
}

constexpr std::array kMultiStateFields{
    FieldRule<MultiStateElement>{.id = 0x28, .read = ReadImageSet},
    Field<&MultiStateElement::fonts>(0x29),
};

Result<void> ReadNestedButton(TokenReader& reader, HotspotElement& element) {
    NestedButton& nested = element.button.emplace();
    return ReadBody<ButtonElement>(reader, kButtonFields, nested.widget, &nested.control,
                                   nested.button);
}

constexpr std::array kHotspotFields{
    Field<&HotspotElement::highlightImage>(0x28),
    FieldRule<HotspotElement>{.id = 0x3B, .read = ReadNestedButton},
};

constexpr std::array kSelectorFields{
    Field<&SelectorElement::previousButton, &SelectorElement::nextButton>(0x3B),
    Field<&SelectorElement::frame>(0x3A),
    Field<&SelectorElement::takesUnfocusedInput>(0x33),
};

Result<void> ReadSlots(TokenReader& reader, ModelCarouselElement& element) {
    if (element.slotCount <= 0) {
        return Fail(ErrorCode::InvalidData, "model carousel slots come before their count");
    }
    element.slots.clear();
    for (std::int32_t i = 0; i < element.slotCount; ++i) {
        Result<void> read = ReadValue(reader, element.slots.emplace_back());
        if (!read) {
            return read;
        }
    }
    return {};
}

constexpr std::array kModelCarouselFields{
    Field<&ModelCarouselElement::slotCount, &ModelCarouselElement::focusedSlot,
          &ModelCarouselElement::scrollStep>(0x33),
    FieldRule<ModelCarouselElement>{.id = 0x2F, .read = ReadSlots},
    Field<&ModelCarouselElement::camera>(0x2E),
};

constexpr std::array kSliderFields{
    Field<&SliderElement::stepCount, &SliderElement::startValue>(0x33),
    Field<&SliderElement::previousButton, &SliderElement::nextButton>(0x3B),
    Field<&SliderElement::track, &SliderElement::thumb>(0x38),
    Field<&SliderElement::images>(0x28),
    Field<&SliderElement::soundIds>(0x2B),
};

constexpr std::array kSceneFields{
    Field<&SceneElement::world>(0x2D),     Field<&SceneElement::linkedElement>(0x3A),
    Field<&SceneElement::camera>(0x2E),    Field<&SceneElement::worldFlags>(0x33),
    Field<&SceneElement::clearMode>(0x35),
};

constexpr std::array kTextFieldFields{
    Field<&TextFieldElement::font>(0x29),
    Field<&TextFieldElement::maxLength>(0x33),
    Field<&TextFieldElement::soundIds>(0x2B),
};

constexpr std::array kCutsceneFields{
    Field<&CutsceneElement::playback>(0x33),
    Field<&CutsceneElement::cutscene>(0x2D),
};

constexpr std::array kGameButtonFields{
    Field<&GameButtonElement::fonts>(0x29),
    Field<&GameButtonElement::images>(0x28),
    Field<&GameButtonElement::colors>(0x2A),
    Field<&GameButtonElement::maxTextWidth>(0x33),
};

using ReadElement = Result<void> (*)(TokenReader& reader, MenuElement& element);

template <typename Body, const auto& Rules>
Result<void> ReadElementBody(TokenReader& reader, MenuElement& element) {
    MenuControl* control = element.control ? &*element.control : nullptr;
    return ReadBody<Body>(reader, Rules, element.widget, control, element.body.emplace<Body>());
}

Result<void> ReadRegion(TokenReader& reader, MenuElement& element) {
    element.body.emplace<RegionElement>();
    return ReadValue(reader, element.widget);
}

struct ElementKind {
    FieldId tag = 0;
    bool isControl = false;
    ReadElement read = nullptr;
};

constexpr std::array kElementKinds{
    ElementKind{.tag = 0x37, .read = ReadRegion},
    ElementKind{.tag = 0x38, .read = ReadElementBody<ImageElement, kImageFields>},
    ElementKind{.tag = 0x39, .read = ReadElementBody<TextLabelElement, kTextLabelFields>},
    ElementKind{.tag = 0x3A, .read = ReadElementBody<FrameElement, kFrameFields>},
    ElementKind{
        .tag = 0x3B, .isControl = true, .read = ReadElementBody<ButtonElement, kButtonFields>},
    ElementKind{.tag = 0x3C,
                .isControl = true,
                .read = ReadElementBody<MultiStateElement, kMultiStateFields>},
    ElementKind{
        .tag = 0x3D, .isControl = true, .read = ReadElementBody<HotspotElement, kHotspotFields>},
    ElementKind{
        .tag = 0x3E, .isControl = true, .read = ReadElementBody<SelectorElement, kSelectorFields>},
    ElementKind{.tag = 0x3F, .read = ReadElementBody<ModelCarouselElement, kModelCarouselFields>},
    ElementKind{
        .tag = 0x40, .isControl = true, .read = ReadElementBody<SliderElement, kSliderFields>},
    ElementKind{.tag = 0x42, .read = ReadElementBody<SceneElement, kSceneFields>},
    ElementKind{.tag = 0x43,
                .isControl = true,
                .read = ReadElementBody<TextFieldElement, kTextFieldFields>},
    ElementKind{.tag = 0x45, .read = ReadElementBody<CutsceneElement, kCutsceneFields>},
    ElementKind{.tag = 0x46,
                .isControl = true,
                .read = ReadElementBody<GameButtonElement, kGameButtonFields>},
};

std::vector<std::string_view> ElementReferences(const MenuElementBody& body) {
    if (const auto* selector = std::get_if<SelectorElement>(&body)) {
        return {selector->previousButton, selector->nextButton, selector->frame};
    }
    if (const auto* slider = std::get_if<SliderElement>(&body)) {
        return {slider->previousButton, slider->nextButton, slider->track, slider->thumb};
    }
    if (const auto* scene = std::get_if<SceneElement>(&body)) {
        return {scene->linkedElement};
    }
    return {};
}

class LayoutBuilder {
public:
    explicit LayoutBuilder(std::int32_t capacity) { layout_.capacity = capacity; }

    Result<void> Add(MenuElement element) {
        if (std::ssize(layout_.elements) >= layout_.capacity) {
            return Fail(ErrorCode::InvalidData,
                        std::format("menu holds over {} elements", layout_.capacity));
        }
        for (const std::string_view reference : ElementReferences(element.body)) {
            if (!reference.empty() && layout_.Find(reference) == nullptr) {
                return Fail(ErrorCode::InvalidData,
                            std::format("menu element {} refers to {} before it's defined",
                                        element.name, reference));
            }
        }
        element.id = element.widget.id.value_or(nextId_);
        if (element.id < 0 || element.id >= kMaxElementId) {
            return Fail(ErrorCode::InvalidData,
                        std::format("menu element {} has id {}, outside 0 to {}", element.name,
                                    element.id, kMaxElementId - 1));
        }
        nextId_ = element.id + 1;
        layout_.elements.push_back(std::move(element));
        return {};
    }

    [[nodiscard]] MenuLayout Take() {
        for (MenuElement& element : layout_.elements) {
            const MenuElement* parent =
                element.widget.parent.empty() ? nullptr : layout_.Find(element.widget.parent);
            element.parentId = parent == nullptr ? kRootElementId : parent->id;
        }
        return std::move(layout_);
    }

private:
    MenuLayout layout_;
    std::int32_t nextId_ = kFirstElementId;
};

Result<void> ReadSection(TokenReader& reader, LayoutBuilder& builder) {
    Result<Token> tag = reader.Expect(TokenKind::Field);
    if (!tag) {
        return std::unexpected(std::move(tag.error()));
    }
    const auto kind = std::ranges::find(kElementKinds, tag->field, &ElementKind::tag);
    if (kind == kElementKinds.end()) {
        return Fail(ErrorCode::InvalidData, std::format("unknown menu element type 0x{:02X} at "
                                                        "offset {}",
                                                        tag->field, tag->offset));
    }
    return ReadList(reader, [&] {
        MenuElement element;
        if (kind->isControl) {
            element.control.emplace();
        }
        return ExpectField(reader, kind->tag)
            .and_then([&] { return reader.ReadString(); })
            .and_then([&](std::string_view name) {
                element.name = name;
                return kind->read(reader, element);
            })
            .and_then([&] { return builder.Add(std::move(element)); });
    });
}

}

static Result<void> ReadValue(TokenReader& reader, MenuWidget& widget) {
    return ReadBlock<MenuWidget>(reader, kWidgetFields, widget);
}

const MenuElement* MenuLayout::Find(std::string_view name) const {
    const auto element = std::ranges::find(elements, NameKey(name),
                                           [](const MenuElement& e) { return NameKey(e.name); });
    return element == elements.end() ? nullptr : &*element;
}

std::vector<std::string_view> ImageNames(const MenuElementBody& body) {
    std::vector<std::string_view> names;
    const auto add = [&](std::span<const std::string> images) {
        names.insert(names.end(), images.begin(), images.end());
    };
    const auto addOptional = [&](const std::optional<StateImages>& images) {
        if (images) {
            add(*images);
        }
    };
    if (const auto* image = std::get_if<ImageElement>(&body)) {
        names.push_back(image->image);
    } else if (const auto* frame = std::get_if<FrameElement>(&body)) {
        add(frame->images);
    } else if (const auto* button = std::get_if<ButtonElement>(&body)) {
        add(button->images);
    } else if (const auto* multiState = std::get_if<MultiStateElement>(&body)) {
        for (const StateImages& images : multiState->imageSets) {
            add(images);
        }
    } else if (const auto* hotspot = std::get_if<HotspotElement>(&body)) {
        names.push_back(hotspot->highlightImage);
        if (hotspot->button) {
            add(hotspot->button->button.images);
        }
    } else if (const auto* slider = std::get_if<SliderElement>(&body)) {
        addOptional(slider->images);
    } else if (const auto* gameButton = std::get_if<GameButtonElement>(&body)) {
        addOptional(gameButton->images);
    }
    std::erase(names, std::string_view{});
    return names;
}

Result<MenuLayout> ParseMenuLayout(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    Result<std::int32_t> capacity =
        ExpectField(reader, kCapacityField).and_then([&] { return reader.ReadInt(); });
    if (!capacity) {
        return std::unexpected(std::move(capacity.error()));
    }
    LayoutBuilder builder(*capacity);
    while (true) {
        Result<Token> next = reader.Peek();
        if (!next) {
            return std::unexpected(std::move(next.error()));
        }
        if (next->kind == TokenKind::End) {
            return builder.Take();
        }
        Result<void> section = ReadSection(reader, builder);
        if (!section) {
            return std::unexpected(std::move(section.error()));
        }
    }
}

}
