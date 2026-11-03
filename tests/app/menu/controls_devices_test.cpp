#include "app/menu/controls_devices.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <vector>

#include "app/menu/menu_files.hpp"
#include "assets/archive_survey.hpp"

namespace rr::app {
namespace {

using enum InputSet;

TEST_CASE("the carousel lists up to two pads, then the three keyboard sets") {
    const std::vector<AttachedPad> pads{{.id = 9, .image = DeviceImage::Wheel},
                                        {.id = 4, .image = DeviceImage::Gamepad},
                                        {.id = 6, .image = DeviceImage::Joystick}};

    const std::vector<DeviceEntry> entries = DeviceEntries(pads);

    REQUIRE(entries.size() == 5);
    CHECK(entries[0].set == Pad1);
    CHECK(entries[0].image == DeviceImage::Wheel);
    CHECK(entries[0].pad == 9);
    CHECK(entries[1].set == Pad2);
    CHECK(entries[1].pad == 4);
    CHECK(entries[2].set == Keys1);
    CHECK(entries[4].set == Keys3);
    CHECK(entries[4].image == DeviceImage::Keyboard);
    CHECK(DeviceEntries({}).size() == 3);
}

TEST_CASE("wheels and sticks get their own images and every other pad is a gamepad") {
    CHECK(ImageOf(SDL_JOYSTICK_TYPE_WHEEL) == DeviceImage::Wheel);
    CHECK(ImageOf(SDL_JOYSTICK_TYPE_FLIGHT_STICK) == DeviceImage::Joystick);
    CHECK(ImageOf(SDL_JOYSTICK_TYPE_GAMEPAD) == DeviceImage::Gamepad);
    CHECK(ImageOf(SDL_JOYSTICK_TYPE_UNKNOWN) == DeviceImage::Gamepad);
}

TEST_CASE("only keyboard sets rebind steering") {
    CHECK(RebindsSteering(Keys2));
    CHECK_FALSE(RebindsSteering(Pad1));
}

TEST_CASE("leaving stores the device and moves the other player off it") {
    const std::vector<SettingChange> changes = DeviceChanges({Pad1, Pad2}, 0, Keys1, 1);

    REQUIRE(changes.size() == 2);
    CHECK(changes[0].key == "input.p1.device");
    CHECK(changes[0].value == settings::SettingValue{std::string("keys1")});
    CHECK(changes[1].key == "input.p2.device");
    CHECK(changes[1].value == settings::SettingValue{std::string("pad1")});
}

TEST_CASE("leaving on the same device or a free one changes only what it must") {
    CHECK(DeviceChanges({Pad1, Pad2}, 0, Pad1, 1).empty());

    const std::vector<SettingChange> changes = DeviceChanges({Pad1, Pad2}, 1, Keys3, 1);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].key == "input.p2.device");
    CHECK(changes[0].value == settings::SettingValue{std::string("keys3")});
}

TEST_CASE("the retail controls screen has an image for every device") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<MenuFiles> files = ReadMenuFiles(*vfs, "CONTROL");

    REQUIRE(files);
    for (const std::string_view name : kDeviceImageNames) {
        CHECK_MESSAGE(files->FindImage(name) != nullptr, name);
    }
}

}
}
