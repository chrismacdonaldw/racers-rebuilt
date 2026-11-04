#include "app/menu/part_carousel.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <utility>

namespace rr::app {
namespace {

std::span<const assets::DriverPart> ShelfOf(const assets::DriverPartCatalog& catalog,
                                            DriverSlot slot) {
    switch (slot) {
        case DriverSlot::Hat:
            return catalog.hats;
        case DriverSlot::Face:
            return catalog.faces.parts;
        case DriverSlot::Torso:
            return catalog.torsos.parts;
        case DriverSlot::Legs:
            return catalog.legs.parts;
    }
    return {};
}

struct SlotFields {
    std::uint8_t assets::DriverLook::* index;
    std::string assets::DriverLook::* name;
};

SlotFields FieldsOf(DriverSlot slot) {
    switch (slot) {
        case DriverSlot::Hat:
            return {.index = &assets::DriverLook::hat, .name = &assets::DriverLook::hatName};
        case DriverSlot::Face:
            return {.index = &assets::DriverLook::face, .name = &assets::DriverLook::faceName};
        case DriverSlot::Torso:
            return {.index = &assets::DriverLook::torso, .name = &assets::DriverLook::torsoName};
        case DriverSlot::Legs:
            return {.index = &assets::DriverLook::legs, .name = &assets::DriverLook::legsName};
    }
    return {.index = &assets::DriverLook::hat, .name = &assets::DriverLook::hatName};
}

std::ptrdiff_t Wrap(std::ptrdiff_t index, std::size_t count) {
    const auto size = static_cast<std::ptrdiff_t>(count);
    return ((index % size) + size) % size;
}

}

std::uint8_t PartOf(const assets::DriverLook& look, DriverSlot slot) {
    return look.*FieldsOf(slot).index;
}

void PickPart(assets::DriverLook& look, DriverSlot slot, std::uint8_t part) {
    const SlotFields fields = FieldsOf(slot);
    look.*fields.index = part;
    (look.*fields.name).clear();
}

std::vector<std::uint8_t> UnlockedParts(const assets::DriverPartCatalog& catalog, DriverSlot slot,
                                        std::uint8_t current, const settings::Progress& progress) {
    const std::span<const assets::DriverPart> shelf = ShelfOf(catalog, slot);
    const bool allRacesUnlocked = settings::HasBeatenEveryRecord(progress);
    std::vector<std::uint8_t> parts;
    for (std::size_t part = 0; part < shelf.size() && part <= UINT8_MAX; ++part) {
        if (part == current ||
            assets::IsPartUnlocked(shelf[part].unlock, progress.partSets, allRacesUnlocked)) {
            parts.push_back(static_cast<std::uint8_t>(part));
        }
    }
    return parts;
}

PartCarousel::PartCarousel(std::vector<std::uint8_t> parts, std::uint8_t current,
                           CarouselShape shape, Milliseconds scroll)
    : parts_(std::move(parts)), shape_(shape), scroll_(scroll) {
    const auto found = std::ranges::find(parts_, current);
    selected_ = found == parts_.end() ? 0 : static_cast<std::size_t>(found - parts_.begin());
}

CarouselStep PartCarousel::Step(int direction) {
    if (parts_.empty() || IsScrolling()) {
        return CarouselStep::Ignored;
    }
    const bool isAtEnd = direction > 0 ? selected_ + 1 == parts_.size() : selected_ == 0;
    if (isAtEnd && !Wraps()) {
        return CarouselStep::AtEnd;
    }
    selected_ = static_cast<std::size_t>(
        Wrap(static_cast<std::ptrdiff_t>(selected_) + direction, parts_.size()));
    direction_ = direction;
    left_ = scroll_;
    return CarouselStep::Moved;
}

void PartCarousel::Select(std::size_t index) {
    if (!parts_.empty()) {
        selected_ = index % parts_.size();
    }
    left_ = Milliseconds{0};
}

void PartCarousel::Tick(Milliseconds elapsed) {
    left_ -= std::min(elapsed, left_);
}

std::uint8_t PartCarousel::Part() const {
    return parts_.empty() ? 0 : parts_[selected_];
}

std::optional<std::uint8_t> PartCarousel::PartInSlot(std::size_t slot) const {
    if (parts_.empty()) {
        return std::nullopt;
    }
    const std::ptrdiff_t index =
        static_cast<std::ptrdiff_t>(selected_ + slot) - static_cast<std::ptrdiff_t>(shape_.focused);
    if (Wraps()) {
        return parts_[static_cast<std::size_t>(Wrap(index, parts_.size()))];
    }
    if (index < 0 || std::cmp_greater_equal(index, parts_.size())) {
        return std::nullopt;
    }
    return parts_[static_cast<std::size_t>(index)];
}

std::optional<std::size_t> PartCarousel::CameFrom(std::size_t slot) const {
    const std::ptrdiff_t from = static_cast<std::ptrdiff_t>(slot) + direction_;
    if (!IsScrolling() || from < 0 || std::cmp_greater_equal(from, shape_.slots)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(from);
}

float PartCarousel::Lag() const {
    return scroll_ > Milliseconds{0}
               ? static_cast<float>(left_.count()) / static_cast<float>(scroll_.count())
               : 0.0F;
}

bool PartCarousel::Wraps() const {
    return parts_.size() + 1 >= shape_.slots;
}

}
