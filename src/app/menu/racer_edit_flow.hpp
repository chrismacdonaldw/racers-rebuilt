#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace rr::app {

enum class RacerEditor : std::uint8_t { Driver, Licence, Car };

struct EditedRacer {
    std::size_t index = 0;
    bool isNew = false;
};

struct EditorScene {
    RacerEditor editor = RacerEditor::Driver;
    EditedRacer racer;
};

struct EditorButton {
    std::string_view style;
    std::size_t line = 0;
};

struct EditorButtons {
    EditorButton done;
    EditorButton back;
};

[[nodiscard]] std::string EditorSceneName(RacerEditor editor, EditedRacer racer);
[[nodiscard]] std::optional<EditorScene> ParseEditorScene(std::string_view scene);
[[nodiscard]] EditorButtons ButtonsOf(RacerEditor editor, bool isNew);

/// A new racer goes on through the driver, licence and car editors before the garage.
[[nodiscard]] std::string NextScene(RacerEditor editor, EditedRacer racer);
[[nodiscard]] std::string PreviousScene(RacerEditor editor, EditedRacer racer);

}
