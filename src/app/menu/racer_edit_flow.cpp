#include "app/menu/racer_edit_flow.hpp"

#include <array>

#include "app/menu/menu_scenes.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kEditors = 3;
constexpr std::string_view kDoneStyle = "buttonch";
constexpr std::string_view kBackStyle = "buttonca";
constexpr std::string_view kNextStyle = "buttonra";
constexpr std::string_view kPreviousStyle = "buttonla";
constexpr std::size_t kDoneLine = 0x72;
constexpr std::size_t kBackLine = 0x1F;
constexpr std::size_t kDriverLine = 0x09;
constexpr std::size_t kLicenceLine = 0x0A;
constexpr std::size_t kCarLine = 0x0B;
constexpr std::size_t kFinishLine = 0x1E;

constexpr std::array<std::string_view, kEditors> kEditHeads{kEditDriverScene, kLicenceScene,
                                                            kEditCarScene};
constexpr std::array<std::string_view, kEditors> kNewHeads{kNewDriverScene, kNewLicenceScene,
                                                           kNewCarScene};
constexpr EditorButtons kEditButtons{.done = {.style = kDoneStyle, .line = kDoneLine},
                                     .back = {.style = kBackStyle, .line = kBackLine}};
constexpr std::array<EditorButtons, kEditors> kNewButtons{
    EditorButtons{.done = {.style = kNextStyle, .line = kLicenceLine},
                  .back = {.style = kBackStyle, .line = kBackLine}},
    EditorButtons{.done = {.style = kNextStyle, .line = kCarLine},
                  .back = {.style = kPreviousStyle, .line = kDriverLine}},
    EditorButtons{.done = {.style = kDoneStyle, .line = kFinishLine},
                  .back = {.style = kPreviousStyle, .line = kLicenceLine}}};

std::size_t IndexOf(RacerEditor editor) {
    return static_cast<std::size_t>(editor);
}

std::string GarageOf(EditedRacer racer) {
    return RacerScene(kGarageScene, racer.index);
}

}

std::string EditorSceneName(RacerEditor editor, EditedRacer racer) {
    return RacerScene((racer.isNew ? kNewHeads : kEditHeads).at(IndexOf(editor)), racer.index);
}

std::optional<EditorScene> ParseEditorScene(std::string_view scene) {
    for (std::size_t editor = 0; editor < kEditors; ++editor) {
        for (const bool isNew : {false, true}) {
            const std::string_view head = (isNew ? kNewHeads : kEditHeads).at(editor);
            if (const std::optional<std::size_t> racer = SceneRacer(scene, head)) {
                return EditorScene{.editor = static_cast<RacerEditor>(editor),
                                   .racer = {.index = *racer, .isNew = isNew}};
            }
        }
    }
    return std::nullopt;
}

EditorButtons ButtonsOf(RacerEditor editor, bool isNew) {
    return isNew ? kNewButtons.at(IndexOf(editor)) : kEditButtons;
}

std::string NextScene(RacerEditor editor, EditedRacer racer) {
    if (!racer.isNew || editor == RacerEditor::Car) {
        return GarageOf(racer);
    }
    return EditorSceneName(static_cast<RacerEditor>(IndexOf(editor) + 1), racer);
}

std::string PreviousScene(RacerEditor editor, EditedRacer racer) {
    if (!racer.isNew || editor == RacerEditor::Driver) {
        return GarageOf(racer);
    }
    return EditorSceneName(static_cast<RacerEditor>(IndexOf(editor) - 1), racer);
}

}
