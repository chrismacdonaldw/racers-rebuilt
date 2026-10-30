#include "assets/menu_styles.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <vector>

#include "archive_survey.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

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
        Ints({3}),
        NamedLibrary(0x32, {NamedEntry(0x32, "backdrp", Join({Bytes({0x28}), Text("backdrp")}))}),
        NamedLibrary(0x3D, {NamedEntry(0x3A, "textedit", skipped)}),
        NamedLibrary(0x3E, {NamedEntry(0x3E, "nubutton",
                                       Join({ControlFields(), StateImageNames(), Bytes({0x2E}),
                                             Ints({0})}))}),
    });
}

TEST_CASE("menu styles read image and game button styles and skip the rest") {
    const Result<MenuStyles> styles =
        ParseMenuStyles(Styles(Join({ControlFields(), StateImageNames()})));

    REQUIRE(styles.has_value());
    REQUIRE(styles->FindImage("backdrp") != nullptr);
    CHECK(styles->FindImage("backdrp")->image == "backdrp");
    const GameButtonStyle* button = styles->FindGameButton("nubutton");
    REQUIRE(button != nullptr);
    REQUIRE(button->button.images.has_value());
    const StateImages& images = *button->button.images;
    CHECK(images[2] == "a");
    CHECK(button->value2E == 0);
    CHECK(styles->FindGameButton("textedit") == nullptr);
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
}

}
}
