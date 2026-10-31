#include "sim/physics/hover_contact.hpp"

#include <doctest/doctest.h>

namespace rr::sim {
namespace {

TEST_CASE("a hovering car banks into a turn by up to 45 degrees") {
    CHECK(HoverBankTarget(0.0F) == 0.0F);
    CHECK(HoverBankTarget(4096.0F) == 0.0F);
    CHECK(HoverBankTarget(2048.0F) == doctest::Approx(-0.3926991F));
    CHECK(HoverBankTarget(-40.0F) == doctest::Approx(0.7853982F * (1.0F - (40.0F / 4096.0F))));
}

}
}
