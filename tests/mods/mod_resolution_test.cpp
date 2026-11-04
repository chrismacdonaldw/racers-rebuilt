#include "mods/mod_resolution.hpp"

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <vector>

#include "mod_test_mods.hpp"

namespace rr::mods {
namespace {

using test::IdsOf;
using test::MakeMod;

std::vector<std::string> MountOrder(const std::vector<Mod>& mods, std::vector<bool> isOn = {}) {
    isOn.resize(mods.size(), true);
    return IdsOf(mods, ResolveMods(mods, isOn).mountOrder);
}

std::string HeldReason(const std::vector<Mod>& mods, std::string_view id,
                       std::vector<bool> isOn = {}) {
    isOn.resize(mods.size(), true);
    for (const HeldMod& held : ResolveMods(mods, isOn).held) {
        if (mods[held.index].Manifest().id == id) {
            return held.reason;
        }
    }
    return {};
}

TEST_CASE("without dependencies the lowest priority mounts first") {
    const std::vector mods{MakeMod("high"), MakeMod("middle"), MakeMod("low")};

    CHECK(MountOrder(mods) == std::vector<std::string>{"low", "middle", "high"});
    CHECK(MountOrder(mods, {true, false, true}) == std::vector<std::string>{"low", "high"});
}

TEST_CASE("required and after mods mount below the mods naming them, whatever their priority") {
    const std::vector mods{MakeMod("base"), MakeMod("other"),
                           MakeMod("addon", "[requires]\nbase = \">=1.0.0\"\n"),
                           MakeMod("late", "[after]\nother = \"*\"\n")};

    CHECK(MountOrder(mods) == std::vector<std::string>{"other", "late", "base", "addon"});
    CHECK(MountOrder(mods, {true, false, true, true}) ==
          std::vector<std::string>{"late", "base", "addon"});
}

TEST_CASE("a mod whose requirement isn't met is held back with the reason") {
    const std::vector mods{MakeMod("base", "", "1.1.0"),
                           MakeMod("needs-newer", "[requires]\nbase = \">=1.2.0\"\n"),
                           MakeMod("needs-missing", "[requires]\nbrick-pack = \"^1.2.0\"\n"),
                           MakeMod("needs-held", "[requires]\nneeds-newer = \"*\"\n")};

    CHECK(MountOrder(mods) == std::vector<std::string>{"base"});
    CHECK(HeldReason(mods, "needs-newer") == "Needs base 1.2.0 or newer, has 1.1.0");
    CHECK(HeldReason(mods, "needs-missing") == "Needs brick-pack 1.2.0 or a newer 1.x");
    CHECK(HeldReason(mods, "needs-held") == "Needs needs-newer, which is held back");
}

TEST_CASE("a mod whose requirement is off is held back") {
    const std::vector mods{MakeMod("base"), MakeMod("addon", "[requires]\nbase = \"*\"\n")};

    CHECK(MountOrder(mods, {false, true}).empty());
    CHECK(HeldReason(mods, "addon", {false, true}) == "Needs base, which is off");
}

TEST_CASE("of two conflicting mods the higher priority one loads") {
    const std::vector mods{MakeMod("new-tracks"),
                           MakeMod("old-tracks", "[conflicts]\nnew-tracks = \"*\"\n"),
                           MakeMod("unrelated", "[conflicts]\nnew-tracks = \"=9.0.0\"\n")};

    CHECK(MountOrder(mods) == std::vector<std::string>{"unrelated", "new-tracks"});
    CHECK(HeldReason(mods, "old-tracks") == "Conflicts with new-tracks");
    CHECK(MountOrder(mods, {false, true, true}) ==
          std::vector<std::string>{"unrelated", "old-tracks"});
}

TEST_CASE("a load order cycle holds back every mod in it, and what requires them") {
    const std::vector mods{MakeMod("a", "[after]\nb = \"*\"\n"),
                           MakeMod("b", "[requires]\na = \"*\"\n"),
                           MakeMod("c", "[requires]\nb = \"*\"\n"), MakeMod("d")};

    CHECK(MountOrder(mods) == std::vector<std::string>{"d"});
    CHECK(HeldReason(mods, "a") == "Its load order loops through b");
    CHECK(HeldReason(mods, "b") == "Its load order loops through a");
    CHECK(HeldReason(mods, "c") == "Needs b, which is held back");
}

}
}
