#include "assets/token_writer.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "assets/token_reader.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

TEST_CASE("written tokens read back as the same fields, values and lists") {
    TokenWriter writer;
    writer.Field(0x27);
    writer.OpenList(2);
    REQUIRE(writer.String("RKROAD4").has_value());
    REQUIRE(writer.String("").has_value());
    writer.Close();
    writer.Field(0x1234);
    writer.Int(-70000);
    writer.Float(0.1F);
    const std::vector<std::byte> bytes = std::move(writer).Take();

    TokenReader reader(bytes);
    std::vector<std::string> names;
    CHECK(ExpectField(reader, 0x27).has_value());
    CHECK(ReadValue(reader, names).has_value());
    CHECK(ExpectField(reader, 0x1234).has_value());
    CHECK(reader.ReadInt().value_or(0) == -70000);
    CHECK(reader.ReadFloat().value_or(0.0F) == 0.1F);
    CHECK(reader.Expect(TokenKind::End).has_value());
    CHECK(names == std::vector<std::string>{"RKROAD4", ""});
}

TEST_CASE("a string the reader would reject isn't written") {
    TokenWriter writer;

    CHECK_FALSE(writer.String(std::string(64, 'A')).has_value());
    CHECK_FALSE(writer.String(std::string("A\0B", 3)).has_value());
    CHECK(std::move(writer).Take().empty());
}

}
}
