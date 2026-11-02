#include "app/menu/message_dialog.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>

#include "app/frame_loop.hpp"

namespace rr::app {
namespace {

using sim::MenuAction;

constexpr std::size_t kQuitQuestionLine = 0x75;
constexpr std::size_t kYesLine = 0x73;
constexpr std::size_t kNoLine = 0x74;

sim::InputFrame Pressing(MenuAction action) {
    sim::InputFrame frame;
    frame.menuPressed.set(sim::InputFrame::Index(action));
    return frame;
}

void TickFor(MessageDialog& dialog, Milliseconds duration) {
    for (Milliseconds ticked{}; ticked <= duration; ticked += kSimTick) {
        static_cast<void>(dialog.Tick({}, MenuSounds{}));
    }
}

MessageDialog OpenConfirm() {
    MessageDialog dialog(
        DialogLook{}, kQuitQuestionLine,
        {{.line = kYesLine}, {.line = kNoLine, .style = DialogButtonStyle::Cross}});
    TickFor(dialog, kDialogResize);
    return dialog;
}

TEST_CASE("a confirm box opens on its last button") {
    CHECK(OpenConfirm().Focus() == 1);
}

TEST_CASE("up and down move between a box's buttons and wrap") {
    MessageDialog dialog = OpenConfirm();

    CHECK_FALSE(dialog.Tick(Pressing(MenuAction::Down), MenuSounds{}));
    CHECK(dialog.Focus() == 0);
    CHECK_FALSE(dialog.Tick(Pressing(MenuAction::Up), MenuSounds{}));
    CHECK(dialog.Focus() == 1);
}

TEST_CASE("confirm presses the focused button and closes the box") {
    MessageDialog dialog = OpenConfirm();
    static_cast<void>(dialog.Tick(Pressing(MenuAction::Up), MenuSounds{}));

    CHECK(dialog.Tick(Pressing(MenuAction::Confirm), MenuSounds{}) == 0);
    CHECK_FALSE(dialog.IsClosed());
    TickFor(dialog, kDialogResize);
    CHECK(dialog.IsClosed());
}

TEST_CASE("a box ignores keys while it grows") {
    MessageDialog dialog(DialogLook{}, kQuitQuestionLine, {{.line = kYesLine}});

    CHECK_FALSE(dialog.Tick(Pressing(MenuAction::Confirm), MenuSounds{}));
    TickFor(dialog, kDialogResize);
    CHECK_FALSE(dialog.IsClosed());
}

}
}
