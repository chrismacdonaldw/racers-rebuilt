#include "sim/ai/ai_craft.hpp"

#include <doctest/doctest.h>

#include <string_view>

namespace rr::sim {
namespace {

TEST_CASE("craft letters keep a fixed order and read back in any order") {
    const AiCraft passesAndAims{.passes = true, .aims = true};

    CHECK(CraftLetters(AiCraft::All()) == "bpualg");
    CHECK(CraftLetters(passesAndAims) == "pa");
    CHECK(CraftLetters({}).empty());
    CHECK(ParseCraftLetters("ap") == passesAndAims);
    CHECK(ParseCraftLetters(CraftLetters(AiCraft::All())) == AiCraft::All());
}

TEST_CASE("craft letters reject empty, unknown and repeated letters") {
    for (const std::string_view letters : {"", "x", "bb", "bpualgx"}) {
        CAPTURE(letters);
        CHECK_FALSE(ParseCraftLetters(letters).has_value());
    }
}

}
}
