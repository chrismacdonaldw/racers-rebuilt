#include "sim/physics/hover_contact.hpp"

#include <doctest/doctest.h>

namespace rr::sim {
namespace {

TEST_CASE("a hovering car banks into a turn by up to 45 degrees") {
    const VersionRules rules;

    CHECK(HoverBankTarget(0.0F, rules) == 0.0F);
    CHECK(HoverBankTarget(4096.0F, rules) == 0.0F);
    CHECK(HoverBankTarget(2048.0F, rules) == doctest::Approx(-0.3926991F));
    CHECK(HoverBankTarget(-40.0F, rules) ==
          doctest::Approx(0.7853982F * (1.0F - (40.0F / 4096.0F))));
}

TEST_CASE("in 2001 a hovering car banks hard until the turn is far wider than 40") {
    const VersionRules& rules = RulesFor(assets::GameVersion::Release2001);

    CHECK(HoverBankTarget(40.0F, rules) == 0.0F);
    CHECK(HoverBankTarget(80.0F, rules) == doctest::Approx(-0.3926991F));
    CHECK(HoverBankTarget(-4096.0F, rules) ==
          doctest::Approx(0.7853982F * (1.0F - (40.0F / 4096.0F))));
}

}
}
