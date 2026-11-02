#include "assets/menu_styles.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <vector>

#include "archive_survey.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Block;
using test::Bytes;
using test::Ints;
using test::Join;
using test::NamedEntry;
using test::NamedLibrary;
using test::Text;

std::vector<std::byte> StateImageNames() {
    return Join({Bytes({0x28}), Text("u"), Text("s"), Text("a"), Text("g"), Text("d"), Text("x")});
}

std::vector<std::byte> ControlFields() {
    return Join({Bytes({0x3A, 0x05, 0x2B}), Ints({14, 0, 0, 1, 0}), Bytes({0x06})});
}

std::vector<std::byte> Styles(const std::vector<std::byte>& skipped) {
    return Join({
        Bytes({0x27}),
        Ints({4}),
        NamedLibrary(0x32, {NamedEntry(0x32, "backdrp", Join({Bytes({0x28}), Text("backdrp")}))}),
        NamedLibrary(
            0x33, {NamedEntry(0x33, "menufont",
                              Join({Bytes({0x29}), Text("font_ths"), Bytes({0x2F}), Ints({1})}))}),
        NamedLibrary(0x3D, {NamedEntry(0x3A, "textedit", skipped)}),
        NamedLibrary(0x38, {NamedEntry(0x38, "picker",
                                       Join({Bytes({0x2B}), Ints({24, 24, 5}), Bytes({0x2D}),
                                             Ints({250}), Bytes({0x2F}), Ints({1, 1})}))}),
        NamedLibrary(0x39, {NamedEntry(0x39, "textcont",
                                       Join({ControlFields(), Bytes({0x34}), Text("clearbox"),
                                             Bytes({0x35}), Text("clarrow"), Text("crarrow")}))}),
        NamedLibrary(0x3E, {NamedEntry(0x3E, "nubutton",
                                       Join({ControlFields(), StateImageNames(), Bytes({0x2E}),
                                             Ints({0})}))}),
    });
}

TEST_CASE("menu styles read image, text and game button styles and skip the rest") {
    const Result<MenuStyles> styles =
        ParseMenuStyles(Styles(Join({ControlFields(), StateImageNames()})));

    REQUIRE(styles.has_value());
    REQUIRE(styles->FindImage("backdrp") != nullptr);
    CHECK(styles->FindImage("backdrp")->image == "backdrp");
    const TextStyle* text = styles->FindText("menufont");
    REQUIRE(text != nullptr);
    CHECK(text->font == "font_ths");
    CHECK(text->centered == 1);
    CHECK_FALSE(text->color.has_value());
    const GameButtonStyle* button = styles->FindGameButton("nubutton");
    REQUIRE(button != nullptr);
    REQUIRE(button->button.images.has_value());
    const StateImages& images = *button->button.images;
    CHECK(images[2] == "a");
    CHECK(button->value2E == 0);
    CHECK(styles->FindGameButton("textedit") == nullptr);
}

TEST_CASE("menu styles read the sounds of game buttons, selectors and carousels") {
    const Result<MenuStyles> styles =
        ParseMenuStyles(Styles(Join({ControlFields(), StateImageNames()})));

    REQUIRE(styles.has_value());
    const GameButtonStyle* button = styles->FindGameButton("nubutton");
    REQUIRE(button != nullptr);
    CHECK(button->icon.sounds == SoundIds{14, 0, 0, 1, 0});
    const SelectorStyle* selector = styles->FindSelector("textcont");
    REQUIRE(selector != nullptr);
    CHECK(selector->icon.sounds == SoundIds{14, 0, 0, 1, 0});
    CHECK(selector->frame == "clearbox");
    CHECK(selector->arrows[1] == "crarrow");
    const CarouselStyle* carousel = styles->FindCarousel("picker");
    REQUIRE(carousel != nullptr);
    CHECK(carousel->sounds == CarouselSounds{24, 24, 5});
    CHECK(carousel->scrollMs == 250);
    CHECK(carousel->wraps == 1);
}

TEST_CASE("a style's control fields with an unknown field fail") {
    const std::vector<std::byte> unknown =
        Join({Bytes({0x3A}), Block(Join({Bytes({0x31}), Ints({1})}))});
    CHECK_FALSE(ParseMenuStyles(Join({Bytes({0x27}), Ints({1}),
                                      NamedLibrary(0x3E, {NamedEntry(0x3E, "nubutton", unknown)})}))
                    .has_value());
}

TEST_CASE("a skipped style without its closing brace fails") {
    CHECK_FALSE(ParseMenuStyles(Styles(Bytes({0x3A, 0x05}))).has_value());
}

void CheckAllParse(const JamArchive& archive) {
    const std::vector<IndexEntry> files = test::FilesWithExtension(archive, "MSB");
    for (const IndexEntry& entry : files) {
        CHECK_MESSAGE(test::ParseEntry(archive, entry, ParseMenuStyles).has_value(), entry.path);
    }
    CHECK(files.size() == 9);
}

void CheckMainMenuButtonStyle(const MenuStyles& shared) {
    const GameButtonStyle* button = shared.FindGameButton("nubutton");
    REQUIRE(button != nullptr);
    REQUIRE(button->button.colors.has_value());
    const auto& colors = *button->button.colors;
    CHECK(colors[11] == MenuColor{.alpha = 255, .red = 246, .green = 230, .blue = 6});
}

void CheckControlSounds(const MenuStyles& shared) {
    const GameButtonStyle* ok = shared.FindGameButton("buttonch");
    REQUIRE(ok != nullptr);
    CHECK(ok->icon.sounds == SoundIds{14, 0, 0, 19, 0});
    const SelectorStyle* selector = shared.FindSelector("imagecon");
    REQUIRE(selector != nullptr);
    CHECK(selector->icon.sounds == SoundIds{14, 0, 0, 0, 0});
}

void CheckPickerSounds(const MenuStyles& shared) {
    const CarouselStyle* picker = shared.FindCarousel("picker");
    REQUIRE(picker != nullptr);
    CHECK(picker->sounds == CarouselSounds{24, 24, 5});
    CHECK(picker->wraps == 1);
}

void CheckMenuFontStyle(const MenuStyles& shared) {
    const TextStyle* text = shared.FindText("menufont");
    REQUIRE(text != nullptr);
    CHECK(text->font == "font_ths");
    CHECK(text->color == MenuColor{.alpha = 255, .red = 239, .green = 239, .blue = 239});
}

TEST_CASE("every menu style file in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }
    CheckAllParse(*archive);

    const Result<MenuStyles> shared =
        test::ReadRetailFile(*archive, "MENUDATA/GSTYLES.MSB").and_then([](const FileData& file) {
            return ParseMenuStyles(file.Bytes());
        });
    REQUIRE(shared.has_value());
    CheckMainMenuButtonStyle(*shared);
    CheckMenuFontStyle(*shared);
    CheckControlSounds(*shared);
    CheckPickerSounds(*shared);
}

}
}
