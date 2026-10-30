#include "assets/menu_layout.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "archive_survey.hpp"
#include "assets/game_path.hpp"
#include "assets/image_list.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Ints;
using test::Join;
using test::Text;

std::vector<std::byte> StateImageNames() {
    return Join({Bytes({0x28}), Text("u"), Text("s"), Text("a"), Text("g"), Text("d"), Text("x")});
}

std::vector<std::byte> Section(std::uint8_t tag, std::uint8_t count,
                               const std::vector<std::byte>& elements) {
    return Join({Bytes({tag}), test::ListHeader(count), elements, Bytes({0x06})});
}

std::vector<std::byte> Element(std::uint8_t tag, std::string_view name,
                               const std::vector<std::byte>& fields) {
    return Join({Bytes({tag}), Text(name), Bytes({0x05}), fields, Bytes({0x06})});
}

std::vector<std::byte> Layout(std::uint8_t capacity, std::string_view selectorFrame,
                              std::string_view nextParent = "selector") {
    return Join({
        Bytes({0x27}),
        Ints({capacity}),
        Section(0x38, 1,
                Element(0x38, "backdrop",
                        Join({Bytes({0x36, 0x05, 0x2F}), Ints({0, 0, 64, 48}), Bytes({0x06, 0x28}),
                              Text("backdrp")}))),
        Section(
            0x3B, 2,
            Join({Element(0x3B, "previous",
                          Join({StateImageNames(), Bytes({0x36, 0x05, 0x31}), Text("backdrop"),
                                Bytes({0x06, 0x2C}), Ints({3})})),
                  Element(0x3B, "next",
                          Join({StateImageNames(), Bytes({0x33}), Ints({0}),
                                Bytes({0x36, 0x05, 0x31}), Text(nextParent), Bytes({0x06})}))})),
        Section(0x3E, 1,
                Element(0x3E, "selector",
                        Join({Bytes({0x3B}), Text("previous"), Text("next"), Bytes({0x3A}),
                              Text(selectorFrame)}))),
    });
}

TEST_CASE("a menu layout numbers its elements and resolves parents and references") {
    const Result<MenuLayout> layout = ParseMenuLayout(Layout(4, "backdrop"));

    REQUIRE(layout.has_value());
    REQUIRE(layout->elements.size() == 4);
    const MenuElement& backdrop = layout->elements[0];
    CHECK(backdrop.id == 2);
    CHECK(backdrop.parentId == kRootElementId);
    CHECK(backdrop.widget.rect == MenuRect{.right = 64, .bottom = 48});
    CHECK_FALSE(backdrop.control.has_value());
    CHECK(ImageNames(backdrop.body) == std::vector<std::string_view>{"backdrp"});

    const MenuElement& previous = layout->elements[1];
    CHECK(previous.id == 3);
    CHECK(previous.parentId == 2);
    REQUIRE(previous.control.has_value());
    const MenuControl& previousControl = *previous.control;
    CHECK(previousControl.helpStringId == 3);
    CHECK(previousControl.startsEnabled == 1);
    CHECK(ImageNames(previous.body).size() == 6);
    const MenuElement& next = layout->elements[2];
    REQUIRE(next.control.has_value());
    const MenuControl& nextControl = *next.control;
    CHECK(nextControl.startsEnabled == 0);
    CHECK(next.parentId == 5);

    const MenuElement& selector = layout->elements[3];
    CHECK(selector.id == 5);
    REQUIRE(std::holds_alternative<SelectorElement>(selector.body));
    CHECK(std::get<SelectorElement>(selector.body).nextButton == "next");
    CHECK(layout->Find("backdropX") == &backdrop);
    CHECK(layout->Find("missing") == nullptr);
}

TEST_CASE("a parent missing from the layout is the root") {
    const Result<MenuLayout> layout = ParseMenuLayout(Layout(4, "backdrop", "garage"));

    REQUIRE(layout.has_value());
    CHECK(layout->elements[2].parentId == kRootElementId);
}

TEST_CASE("a menu layout fails over capacity or with a reference before its target") {
    CHECK_FALSE(ParseMenuLayout(Layout(3, "backdrop")).has_value());
    CHECK_FALSE(ParseMenuLayout(Layout(4, "selector")).has_value());
}

TEST_CASE("a menu element rejects fields its type doesn't have") {
    const auto parse = [](std::uint8_t tag, const std::vector<std::byte>& fields) {
        return ParseMenuLayout(
            Join({Bytes({0x27}), Ints({1}), Section(tag, 1, Element(tag, "a", fields))}));
    };

    CHECK(parse(0x38, Join({Bytes({0x28}), Text("a")})).has_value());
    CHECK_FALSE(parse(0x38, Join({Bytes({0x2C}), Ints({1})})).has_value());
    CHECK(parse(0x3B, Join({Bytes({0x2C}), Ints({1})})).has_value());
    CHECK_FALSE(parse(0x41, {}).has_value());
}

std::vector<std::byte> Int32(std::int32_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    return Bytes({0x04, static_cast<std::uint8_t>(bits), static_cast<std::uint8_t>(bits >> 8U),
                  static_cast<std::uint8_t>(bits >> 16U), static_cast<std::uint8_t>(bits >> 24U)});
}

std::vector<std::byte> OneElement(std::uint8_t tag, const std::vector<std::byte>& fields) {
    return Join({Bytes({0x27}), Ints({1}), Section(tag, 1, Element(tag, "a", fields))});
}

TEST_CASE("a huge model carousel slot count fails without allocating the slots") {
    CHECK_FALSE(ParseMenuLayout(
                    OneElement(0x3F, Join({Bytes({0x33}), Int32(0x7FFFFFFF), Ints({0}),
                                           test::Floats({1}), Bytes({0x2F}), Ints({0, 0, 1, 1})})))
                    .has_value());
}

TEST_CASE("an element id outside the 16-bit range fails") {
    const auto withId = [](std::int32_t id) {
        return ParseMenuLayout(OneElement(0x37, Join({Bytes({0x30}), Int32(id)})));
    };

    CHECK(withId(32766).has_value());
    CHECK_FALSE(withId(32767).has_value());
    CHECK_FALSE(withId(0x7FFFFFFF).has_value());
    CHECK_FALSE(withId(-1).has_value());
}

std::set<std::string> MenuImageNames(const JamArchive& archive) {
    std::set<std::string> names;
    for (const IndexEntry& imageList : test::FilesWithExtension(archive, "IDB")) {
        if (imageList.Parent() != "MENUDATA") {
            continue;
        }
        const Result<std::vector<ImageListEntry>> images =
            test::ParseEntry(archive, imageList, ParseImageList);
        REQUIRE(images.has_value());
        for (const ImageListEntry& image : *images) {
            names.insert(ToUpperAscii(image.name));
        }
    }
    return names;
}

void CheckImagesListed(const MenuLayout& layout, const std::set<std::string>& images) {
    for (const MenuElement& element : layout.elements) {
        for (const std::string_view image : ImageNames(element.body)) {
            CHECK_MESSAGE(images.contains(ToUpperAscii(image)), image);
        }
    }
}

TEST_CASE("every menu layout in the retail game parses and finds its menu images") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    const std::set<std::string> images = MenuImageNames(*archive);

    const std::vector<IndexEntry> layouts = test::FilesWithExtension(*archive, "MIB");
    std::size_t elementCount = 0;
    for (const IndexEntry& entry : layouts) {
        CAPTURE(entry.path);
        const Result<MenuLayout> layout = test::ParseEntry(*archive, entry, ParseMenuLayout);
        REQUIRE(layout.has_value());
        CheckImagesListed(*layout, images);
        elementCount += layout->elements.size();
    }
    CHECK(layouts.size() == 31);
    CHECK(elementCount == 287);
}

}
}
