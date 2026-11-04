#include "mods/mod_toggles.hpp"

#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

#include "mod_test_mods.hpp"

namespace rr::mods {
namespace {

using test::MakeMod;
using Switched = std::vector<std::pair<std::string, bool>>;

Switched Named(const std::vector<Mod>& mods, const std::vector<ModSwitch>& switches) {
    Switched named;
    for (const ModSwitch& change : switches) {
        named.emplace_back(mods[change.index].Manifest().id, change.isOn);
    }
    return named;
}

TEST_CASE("turning a mod on turns on what it requires, without asking") {
    const std::vector mods{MakeMod("base"), MakeMod("middle", "[requires]\nbase = \"*\"\n"),
                           MakeMod("top", "[requires]\nmiddle = \"*\"\nmissing = \"*\"\n")};

    const ModToggle toggle = PlanModToggle(mods, {false, false, false}, 2);

    CHECK(toggle.question.empty());
    REQUIRE(toggle.choices.size() == 1);
    CHECK(Named(mods, toggle.choices.front().switches) ==
          Switched{{"top", true}, {"middle", true}, {"base", true}});
}

TEST_CASE("turning off a requirement asks before turning off what needs it") {
    const std::vector mods{MakeMod("base"), MakeMod("middle", "[requires]\nbase = \"*\"\n"),
                           MakeMod("top", "[requires]\nmiddle = \"*\"\n"), MakeMod("other")};

    const ModToggle toggle = PlanModToggle(mods, {true, true, true, true}, 0);

    CHECK(toggle.question == "Turning off base also turns off middle, top.");
    REQUIRE(toggle.choices.size() == 2);
    CHECK(Named(mods, toggle.choices[0].switches) ==
          Switched{{"middle", false}, {"top", false}, {"base", false}});
    CHECK(toggle.choices[1].switches.empty());
    CHECK(PlanModToggle(mods, {true, true, true, true}, 3).question.empty());
}

TEST_CASE("turning on a conflicting mod asks which one to keep") {
    const std::vector mods{MakeMod("old"), MakeMod("old-addon", "[requires]\nold = \"*\"\n"),
                           MakeMod("new", "[conflicts]\nold = \"*\"\n")};

    const ModToggle toggle = PlanModToggle(mods, {true, true, false}, 2);

    CHECK(toggle.question == "new conflicts with old.");
    REQUIRE(toggle.choices.size() == 2);
    CHECK(toggle.choices[0].label == "Keep new");
    CHECK(Named(mods, toggle.choices[0].switches) ==
          Switched{{"new", true}, {"old", false}, {"old-addon", false}});
    CHECK(toggle.choices[1].label == "Keep old");
    CHECK(toggle.choices[1].switches.empty());
}

TEST_CASE("required switches turn on what the mods that are on need") {
    const std::vector mods{MakeMod("base"), MakeMod("addon", "[requires]\nbase = \"*\"\n"),
                           MakeMod("off", "[requires]\nbase = \"*\"\n")};

    CHECK(Named(mods, RequiredSwitches(mods, {false, true, false})) == Switched{{"base", true}});
    CHECK(RequiredSwitches(mods, {true, true, false}).empty());
}

}
}
