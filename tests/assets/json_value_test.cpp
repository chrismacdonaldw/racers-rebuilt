#include "assets/json_value.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace rr::assets {
namespace {

TEST_CASE("JSON objects, arrays, numbers and strings read back") {
    const Result<JsonValue> json =
        ParseJson(R"( {"links":["gate.02","gate.03"], "distance": -1.5e2, "on": true} )");

    REQUIRE(json.has_value());
    REQUIRE(json->Find("links") != nullptr);
    CHECK(json->Find("links")->Items().size() == 2);
    CHECK(json->Find("links")->Items()[1].Text() == "gate.03");
    CHECK(json->Find("distance")->Number() == -150.0);
    CHECK(json->Find("missing") == nullptr);
}

TEST_CASE("JSON escapes below U+0100 read as Latin-1 bytes") {
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    const Result<JsonValue> json = ParseJson("\"caf\\u00e9 \\\"q\\\"\\n\"");

    REQUIRE(json.has_value());
    CHECK(json->Text() == std::string_view("caf\xE9 \"q\"\n"));
}

TEST_CASE("JSON numbers convert only when they fit") {
    const Result<JsonValue> json = ParseJson("[300, 2.5, -1, 1e39]");

    REQUIRE(json.has_value());
    const std::span<const JsonValue> items = json->Items();
    REQUIRE(items.size() == 4);
    CHECK(items[0].Integer<std::int32_t>() == 300);
    CHECK_FALSE(items[0].Integer<std::uint8_t>().has_value());
    CHECK_FALSE(items[1].Integer<std::int32_t>().has_value());
    CHECK_FALSE(items[2].Integer<std::size_t>().has_value());
    CHECK(items[1].Float() == 2.5F);
    CHECK_FALSE(items[3].Float().has_value());
}

TEST_CASE("broken JSON fails") {
    const std::string deep(100, '[');
    for (const std::string_view text :
         {std::string_view(R"({"a":1)"), std::string_view(R"({"a" 1})"),
          std::string_view("[1,2] 3"), std::string_view(R"("\x")"), std::string_view("nan"),
          std::string_view("[inf]"), std::string_view(deep)}) {
        CAPTURE(text);
        CHECK_FALSE(ParseJson(text).has_value());
    }
}

}
}
