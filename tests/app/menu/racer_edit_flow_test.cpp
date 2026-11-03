#include "app/menu/racer_edit_flow.hpp"

#include <doctest/doctest.h>

#include <optional>

#include "app/menu/menu_scenes.hpp"

namespace rr::app {
namespace {

constexpr EditedRacer kNew{.index = 4, .isNew = true};
constexpr EditedRacer kEdited{.index = 4};

bool RoundTrips(RacerEditor editor, EditedRacer racer) {
    const std::optional<EditorScene> parsed = ParseEditorScene(EditorSceneName(editor, racer));
    return parsed && parsed->editor == editor && parsed->racer.index == racer.index &&
           parsed->racer.isNew == racer.isNew;
}

bool ReturnsToGarage(RacerEditor editor) {
    const EditorButtons buttons = ButtonsOf(editor, false);
    return NextScene(editor, kEdited) == "menu:garage:4" &&
           PreviousScene(editor, kEdited) == "menu:garage:4" && buttons.done.style == "buttonch" &&
           buttons.done.line == 0x72 && buttons.back.style == "buttonca" &&
           buttons.back.line == 0x1F;
}

TEST_CASE("an editor's scene name reads back as the same editor and racer") {
    CHECK(RoundTrips(RacerEditor::Driver, kNew));
    CHECK(RoundTrips(RacerEditor::Licence, kNew));
    CHECK(RoundTrips(RacerEditor::Car, kNew));
    CHECK(RoundTrips(RacerEditor::Driver, kEdited));
    CHECK(RoundTrips(RacerEditor::Licence, kEdited));
    CHECK(RoundTrips(RacerEditor::Car, kEdited));
    CHECK(EditorSceneName(RacerEditor::Licence, kEdited) == "menu:licence:4");
    CHECK(EditorSceneName(RacerEditor::Licence, kNew) == "menu:new_licence:4");
}

TEST_CASE("other scenes aren't editors") {
    CHECK_FALSE(ParseEditorScene(RacerScene(kGarageScene, 1)).has_value());
    CHECK_FALSE(ParseEditorScene(RacerScene(kCarBuildScene, 1)).has_value());
    CHECK_FALSE(ParseEditorScene(kNewDriverScene).has_value());
}

TEST_CASE("a new racer steps through the driver, licence and car editors") {
    CHECK(NextScene(RacerEditor::Driver, kNew) == "menu:new_licence:4");
    CHECK(NextScene(RacerEditor::Licence, kNew) == "menu:new_car:4");
    CHECK(NextScene(RacerEditor::Car, kNew) == "menu:garage:4");

    CHECK(PreviousScene(RacerEditor::Car, kNew) == "menu:new_licence:4");
    CHECK(PreviousScene(RacerEditor::Licence, kNew) == "menu:new_driver:4");
    CHECK(PreviousScene(RacerEditor::Driver, kNew) == "menu:garage:4");
}

TEST_CASE("editing a racer goes back to the garage either way") {
    CHECK(ReturnsToGarage(RacerEditor::Driver));
    CHECK(ReturnsToGarage(RacerEditor::Licence));
    CHECK(ReturnsToGarage(RacerEditor::Car));
}

TEST_CASE("a new racer's buttons name the step they lead to") {
    CHECK(ButtonsOf(RacerEditor::Driver, true).done.style == "buttonra");
    CHECK(ButtonsOf(RacerEditor::Driver, true).done.line == 0x0A);
    CHECK(ButtonsOf(RacerEditor::Driver, true).back.style == "buttonca");
    CHECK(ButtonsOf(RacerEditor::Licence, true).done.line == 0x0B);
    CHECK(ButtonsOf(RacerEditor::Licence, true).back.style == "buttonla");
    CHECK(ButtonsOf(RacerEditor::Licence, true).back.line == 0x09);
    CHECK(ButtonsOf(RacerEditor::Car, true).done.style == "buttonch");
    CHECK(ButtonsOf(RacerEditor::Car, true).done.line == 0x1E);
    CHECK(ButtonsOf(RacerEditor::Car, true).back.line == 0x0A);
}

}
}
