#include "mods/mod_manifest.hpp"

#include <doctest/doctest.h>

#include <string>
#include <string_view>

namespace rr::mods {
namespace {

std::string ErrorFor(std::string_view text) {
    const Result<ModManifest> manifest = ParseModManifest(text, "mod.toml");
    REQUIRE_FALSE(manifest.has_value());
    return manifest.error().message;
}

TEST_CASE("a manifest reads every field") {
    const Result<ModManifest> manifest = ParseModManifest(
        "id = \"hd-bricks\"\nname = \"HD bricks\"\nversion = \"1.2.0\"\npriority = 5\n"
        "enabled = false\n",
        "mod.toml");

    REQUIRE(manifest.has_value());
    CHECK(manifest->id == "hd-bricks");
    CHECK(manifest->name == "HD bricks");
    CHECK(manifest->version == "1.2.0");
    CHECK(manifest->priority == 5);
    CHECK_FALSE(manifest->isEnabledByDefault);
}

TEST_CASE("priority and enabled are optional") {
    const Result<ModManifest> manifest =
        ParseModManifest("id = \"a\"\nname = \"A\"\nversion = \"1\"\n", "mod.toml");

    REQUIRE(manifest.has_value());
    CHECK(manifest->priority == 0);
    CHECK(manifest->isEnabledByDefault);
}

TEST_CASE("a manifest with a missing or mistyped field is an error") {
    CHECK(ErrorFor("name = \"A\"\nversion = \"1\"\n") == "mod.toml: id is missing");
    CHECK(ErrorFor("id = \"a\"\nname = \"A\"\nversion = 1\n") == "mod.toml: version must be text");
    CHECK(ErrorFor("id = \"a\"\nname = \"A\"\nversion = \"1\"\npriority = 1.5\n") ==
          "mod.toml: priority must be a whole number");
    CHECK(ErrorFor("id = \"a\"\nname = \"A\"\nversion = \"1\"\npriority = 9999999999\n") ==
          "mod.toml: priority must be a whole number");
    CHECK(ErrorFor("id = \"a\"\nname = \"A\"\nversion = \"1\"\nenabled = \"yes\"\n") ==
          "mod.toml: enabled must be true or false");
}

TEST_CASE("an id that can't be a setting key is an error") {
    CHECK(ErrorFor("id = \"HD.Bricks\"\nname = \"A\"\nversion = \"1\"\n").find("may only use") !=
          std::string::npos);
}

TEST_CASE("a manifest that isn't TOML names the line") {
    CHECK(ErrorFor("id = \n").starts_with("mod.toml line 1:"));
}

}
}
