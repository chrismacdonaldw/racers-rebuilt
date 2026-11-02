#include "assets/cutscene_effects.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

std::vector<std::byte> Tagged(std::uint8_t id, const std::vector<std::byte>& value = {}) {
    return test::Join({test::Bytes({id}), value});
}

std::vector<std::byte> Names(std::uint8_t section, std::initializer_list<std::string_view> names) {
    std::vector<std::byte> bytes = test::Join(
        {test::Bytes({section}), test::ListHeader(static_cast<std::uint8_t>(names.size()))});
    for (const std::string_view name : names) {
        bytes = test::Join({bytes, test::Text(name)});
    }
    return test::Join({bytes, test::Bytes({0x06})});
}

std::vector<std::byte> Effect(std::uint8_t section, std::string_view name,
                              const std::vector<std::byte>& fields) {
    return test::NamedLibrary(section, {test::NamedEntry(section, name, fields)});
}

std::vector<std::byte> Link(std::uint8_t channel, std::string_view transform, std::uint8_t section,
                            std::uint8_t mode, std::string_view effect) {
    return test::NamedEntry(channel, transform,
                            test::Join({test::Bytes({section, mode}), test::Text(effect)}));
}

std::vector<std::byte> SoundEffect(const std::vector<std::byte>& fields) {
    return test::Join({Names(0x5C, {"bank"}), Effect(0x2F, "sound", fields)});
}

std::vector<std::byte> TextEffect(const std::vector<std::byte>& fields) {
    return test::Join({Names(0x28, {"english"}),
                       Effect(0x3F, "text",
                              test::Join({Tagged(0x40, test::Ints({0, 1})),
                                          Tagged(0x42, test::Text("font_ths")), fields}))});
}

TEST_CASE("links resolve to the effects they start and stop") {
    const std::vector<std::byte> bytes = test::Join(
        {Names(0x2A, {"splash1"}),
         test::NamedLibrary(
             0x60, {test::NamedEntry(
                        0x60, "FadeIn",
                        test::Join({Tagged(0x61, test::Ints({100})), test::Bytes({0x62, 0x63}),
                                    Tagged(0x66, test::Ints({0, 0, 0}))})),
                    test::NamedEntry(0x60, "Flash",
                                     test::Join({test::Bytes({0x62, 0x64}),
                                                 Tagged(0x66, test::Ints({255, 255, 255}))}))}),
         Effect(0x4D, "Splash1",
                test::Join({Tagged(0x4D, test::Text("Splash1")), test::Bytes({0x45, 0x46})})),
         test::NamedLibrary(0x56, {Link(0x56, "Splash1", 0x4D, 0x4E, "Splash1"),
                                   Link(0x56, "Splash1", 0x60, 0x4E, "Flash")}),
         test::NamedLibrary(0x57, {Link(0x57, "FadeIn", 0x60, 0x4F, "FadeIn")})});

    const Result<CutsceneEffectsAsset> effects = ParseCutsceneEffects(bytes);

    REQUIRE(effects.has_value());
    CHECK(effects->imageLists == std::vector<std::string>{"splash1"});
    REQUIRE(effects->fades.size() == 2);
    CHECK(effects->fades[0].duration == Milliseconds{100});
    CHECK(effects->fades[0].direction == FadeDirection::FromColor);
    CHECK(effects->fades[1].direction == FadeDirection::ToColor);
    CHECK(effects->fades[1].color == CutsceneColor{255, 255, 255});
    REQUIRE(effects->images.size() == 1);
    CHECK(effects->images[0].image == "Splash1");
    CHECK_FALSE(effects->images[0].layout.left.has_value());
    REQUIRE(effects->links.size() == 3);
    CHECK(effects->links[0] == CutsceneLink{.transform = "Splash1",
                                            .onTransformEnd = false,
                                            .kind = EffectKind::Image,
                                            .effect = 0,
                                            .starts = true});
    CHECK(effects->links[1].kind == EffectKind::Fade);
    CHECK(effects->links[1].effect == 1);
    CHECK(effects->links[2] == CutsceneLink{.transform = "FadeIn",
                                            .onTransformEnd = true,
                                            .kind = EffectKind::Fade,
                                            .effect = 0,
                                            .starts = false});
}

TEST_CASE("sounds keep their defaults for fields they leave out") {
    const Result<CutsceneEffectsAsset> effects = ParseCutsceneEffects(
        SoundEffect(test::Join({Tagged(0x30, test::Ints({0, 3})), Tagged(0x35)})));

    REQUIRE(effects.has_value());
    REQUIRE(effects->sounds.size() == 1);
    const CutsceneSound& sound = effects->sounds[0];
    CHECK(sound.bank == 0);
    CHECK(sound.sound == 3);
    CHECK(sound.priority == 0);
    CHECK(sound.volume == 1.0F);
    CHECK(sound.frequencyScale == 1.0F);
    CHECK(sound.pan == 0.0F);
    CHECK(sound.isLooping);
}

TEST_CASE("text keeps its line, font, top and centring") {
    const Result<CutsceneEffectsAsset> effects = ParseCutsceneEffects(TextEffect(
        test::Join({Tagged(0x43, test::Floats({1})), Tagged(0x45), Tagged(0x44, test::Floats({1})),
                    Tagged(0x66, test::Ints({1, 2, 3}))})));

    REQUIRE(effects.has_value());
    REQUIRE(effects->texts.size() == 1);
    const CutsceneText& text = effects->texts[0];
    CHECK(text.line == 1);
    CHECK(text.font == "font_ths");
    CHECK_FALSE(text.layout.left.has_value());
    CHECK(text.layout.top == 1.0F);
    CHECK(text.layout.color == CutsceneColor{1, 2, 3});
}

TEST_CASE("tints and particles bind entities and emitters") {
    const std::vector<std::byte> bytes = test::Join(
        {Names(0x27, {"pst"}),
         Effect(0x2B, "vtx1",
                test::Join(
                    {Tagged(0x2D, test::Ints({64, 0, 0})), Tagged(0x5E, test::Text("mm_sky"))})),
         Effect(0x3C, "snow1",
                test::Join({Tagged(0x3D, test::Join({test::Ints({0}), test::Text("snow")})),
                            Tagged(0x3A), Tagged(0x5D, test::Text("guy1")),
                            Tagged(0x3B, test::Ints({2}))}))});

    const Result<CutsceneEffectsAsset> effects = ParseCutsceneEffects(bytes);

    REQUIRE(effects.has_value());
    REQUIRE(effects->tints.size() == 1);
    CHECK(effects->tints[0].offsets == std::array<std::int32_t, 3>{64, 0, 0});
    CHECK(effects->tints[0].entity == EntityByName{.name = "mm_sky", .isJointed = false});
    REQUIRE(effects->particles.size() == 1);
    CHECK(effects->particles[0].emitter == "snow");
    CHECK(effects->particles[0].entity == EntityByName{.name = "guy1", .isJointed = true});
    CHECK(effects->particles[0].joint == 2);
}

TEST_CASE("a fade without a direction parses without one") {
    const Result<CutsceneEffectsAsset> effects =
        ParseCutsceneEffects(Effect(0x60, "fade", Tagged(0x61, test::Ints({50}))));

    REQUIRE(effects.has_value());
    CHECK_FALSE(effects->fades[0].direction.has_value());
}

TEST_CASE("effects outside the retail slice are rejected") {
    const std::vector<std::byte> sound = SoundEffect(Tagged(0x30, test::Ints({0, 0})));

    SUBCASE("positional sounds") {
        CHECK_FALSE(ParseCutsceneEffects(Effect(0x36, "pos", Tagged(0x30, test::Ints({0, 0})))));
    }
    SUBCASE("font lists") {
        CHECK_FALSE(ParseCutsceneEffects(Names(0x29, {"fonts"})));
    }
    SUBCASE("a BSP entity") {
        CHECK_FALSE(ParseCutsceneEffects(SoundEffect(
            test::Join({Tagged(0x30, test::Ints({0, 0})), Tagged(0x5F, test::Text("bsp"))}))));
    }
    SUBCASE("literal text") {
        CHECK_FALSE(ParseCutsceneEffects(TextEffect(Tagged(0x41, test::Text("hello")))));
    }
    SUBCASE("a visual rate") {
        CHECK_FALSE(ParseCutsceneEffects(TextEffect(Tagged(0x49, test::Floats({1})))));
    }
    SUBCASE("a tint rate") {
        CHECK_FALSE(
            ParseCutsceneEffects(Effect(0x2B, "tint", Tagged(0x2E, test::Floats({1, 1, 1})))));
    }
    SUBCASE("a link channel other than transforms") {
        CHECK_FALSE(ParseCutsceneEffects(test::Join(
            {sound, test::NamedLibrary(0x52, {Link(0x52, "cam", 0x2F, 0x4E, "sound")})})));
    }
    SUBCASE("a link to a missing effect") {
        CHECK_FALSE(ParseCutsceneEffects(
            test::Join({sound, test::NamedLibrary(0x56, {Link(0x56, "t", 0x2F, 0x4E, "other")})})));
    }
    SUBCASE("a link to the wrong kind of effect") {
        CHECK_FALSE(ParseCutsceneEffects(
            test::Join({sound, test::NamedLibrary(0x56, {Link(0x56, "t", 0x60, 0x4E, "sound")})})));
    }
    SUBCASE("a link with an unknown mode") {
        CHECK_FALSE(ParseCutsceneEffects(
            test::Join({sound, test::NamedLibrary(0x56, {Link(0x56, "t", 0x2F, 0x50, "sound")})})));
    }
    SUBCASE("a sound from a missing bank") {
        CHECK_FALSE(ParseCutsceneEffects(Effect(0x2F, "sound", Tagged(0x30, test::Ints({0, 0})))));
    }
    SUBCASE("text without a font") {
        CHECK_FALSE(ParseCutsceneEffects(test::Join(
            {Names(0x28, {"english"}), Effect(0x3F, "text", Tagged(0x40, test::Ints({0, 0})))})));
    }
    SUBCASE("a repeated effect field") {
        CHECK_FALSE(ParseCutsceneEffects(SoundEffect(
            test::Join({Tagged(0x30, test::Ints({0, 0})), Tagged(0x30, test::Ints({0, 1}))}))));
    }
    SUBCASE("an empty section") {
        CHECK_FALSE(ParseCutsceneEffects(
            test::Join({test::Bytes({0x2F}), test::ListHeader(0), test::Bytes({0x06})})));
    }
    SUBCASE("a repeated section") {
        CHECK_FALSE(ParseCutsceneEffects(test::Join({sound, Names(0x5C, {"bank"})})));
    }
}

}
}
