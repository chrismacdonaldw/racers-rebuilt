#include "app/hud/text_pool.hpp"

#include <doctest/doctest.h>

#include <string_view>

namespace rr::app {
namespace {

TEST_CASE("pooled texts stay intact as more are written") {
    TextPool pool;

    const std::u16string_view time = pool.Time(Milliseconds{83456});
    const std::u16string_view number = pool.Number(1205);
    const std::u16string_view zero = pool.Number(0);

    CHECK(time == u"1:23:45");
    CHECK(number == u"1205");
    CHECK(zero == u"0");
}

TEST_CASE("clearing the pool reuses its storage") {
    TextPool pool;
    const std::u16string_view first = pool.Number(7);

    pool.Clear();
    const std::u16string_view second = pool.Number(8);

    CHECK(second.data() == first.data());
    CHECK(second == u"8");
}

}
}
