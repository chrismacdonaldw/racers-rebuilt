#include "overlay/input_gate.hpp"

#include <doctest/doctest.h>

#include "overlay/headless_ui.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace rr::overlay {
namespace {

constexpr settings::SettingSpec<bool> kFreeCamera{
    .key = "enhancements.free_camera", .defaultValue = false, .category = "Enhancements"};

void Click() {
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    ImGui::GetIO().AddMouseButtonEvent(0, false);
}

TEST_CASE("a closed gate queues no events") {
    const HeadlessUi ui;
    const InputGate gate;
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = true;

    gate.Forward(event);

    CHECK(ImGui::GetCurrentContext()->InputEventsQueue.empty());
}

TEST_CASE("a click left over from before opening doesn't apply") {
    settings::Registry registry;
    registry.Declare(kFreeCamera);
    const SettingMenu menu = GroupSettingMenus(registry.AllDeclarations()).front();
    HeadlessUi ui;
    InputGate gate;
    gate.SetOpen(true);
    (void)ui.Frame(registry, menu);
    ui.MoveMouse(0.5F);
    (void)ui.Frame(registry, menu);
    gate.SetOpen(false);

    SUBCASE("replayed without the gate, the click toggles") {
        Click();
        (void)ui.Frame(registry, menu);
        (void)ui.Frame(registry, menu);
        CHECK(registry.Get(kFreeCamera));
    }
    SUBCASE("opening the gate drops it") {
        Click();
        gate.SetOpen(true);
        (void)ui.Frame(registry, menu);
        (void)ui.Frame(registry, menu);
        CHECK_FALSE(registry.Get(kFreeCamera));
    }
}

}
}
