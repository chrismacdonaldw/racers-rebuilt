#include "core/utf16_text.hpp"

#include <doctest/doctest.h>

#include <string>

namespace rr {
namespace {

TEST_CASE("UTF-8 decodes to UTF-16, with surrogate pairs past the first plane") {
    CHECK(Utf16FromUtf8("Basil") == u"Basil");
    CHECK(Utf16FromUtf8("\xC3\x98ster") == u"Øster");
    CHECK(Utf16FromUtf8("\xE2\x82\xAC") == u"€");
    CHECK(Utf16FromUtf8("\xF0\x9F\x8F\x81") == u"\U0001F3C1");
}

TEST_CASE("UTF-8 round trips through UTF-16") {
    const std::string text =
        "R\xC3\xB8"
        "de \xE2\x82\xAC \xF0\x9F\x8F\x81";

    CHECK(Utf8FromUtf16(Utf16FromUtf8(text)) == text);
}

TEST_CASE("broken UTF-8 becomes one replacement per byte that starts no whole sequence") {
    CHECK(Utf16FromUtf8("a\xC3") == u"a�");
    CHECK(Utf16FromUtf8("\xC3x") == u"�x");
    CHECK(Utf16FromUtf8("\xC0\x80") == u"��");
    CHECK(Utf16FromUtf8("\xED\xA0\x80") == u"���");
    CHECK(Utf16FromUtf8("\xF4\x90\x80\x80") == u"����");
}

}
}
