#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/cutscene_timeline.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

struct CutsceneAxes {
    Vector3 direction;
    Vector3 up;

    friend bool operator==(const CutsceneAxes&, const CutsceneAxes&) = default;
};

/// Each vertex color channel becomes `min(255, (c >> shift) + offset)`.
struct CutsceneTint {
    std::string name;
    std::optional<EntityByName> entity;
    std::array<std::int32_t, 3> shifts{};
    std::array<std::int32_t, 3> offsets{};
};

struct CutsceneSound {
    std::string name;
    std::optional<EntityByName> entity;
    std::size_t bank = 0;
    std::size_t sound = 0;
    std::int32_t priority = 0;
    float volume = 1.0F;
    float frequencyScale = 1.0F;
    float pan = 0.0F;
    bool isLooping = false;
};

struct CutsceneParticle {
    std::string name;
    std::size_t library = 0;
    std::string emitter;
    std::optional<EntityByName> entity;
    std::optional<std::int32_t> joint;
    std::optional<Vector3> position;
    std::optional<CutsceneAxes> axes;
};

enum class FadeDirection : std::uint8_t { FromColor, ToColor };

/// A fade without a direction never draws, as in the original.
struct CutsceneFade {
    std::string name;
    Milliseconds duration{0};
    std::optional<FadeDirection> direction;
    std::optional<std::string> material;
    CutsceneColor color{};
};

/// Fractions of the render target. Missing edges centre; missing sizes keep the content's.
struct VisualLayout {
    std::optional<float> left;
    std::optional<float> top;
    std::optional<float> width;
    std::optional<float> height;
    std::optional<CutsceneColor> color;
};

struct CutsceneText {
    std::string name;
    std::size_t stringTable = 0;
    std::size_t line = 0;
    std::string font;
    VisualLayout layout;
};

struct CutsceneImage {
    std::string name;
    std::string image;
    VisualLayout layout;
};

enum class EffectKind : std::uint8_t { Tint, Sound, Particle, Fade, Text, Image };

/// Starts or stops one effect when a timeline transform named `transform` starts or ends.
struct CutsceneLink {
    std::string transform;
    bool onTransformEnd = false;
    EffectKind kind = EffectKind::Sound;
    std::size_t effect = 0;
    bool starts = true;

    friend bool operator==(const CutsceneLink&, const CutsceneLink&) = default;
};

/// Resource names are file names without their extension, looked up in the cutscene folder.
struct CutsceneEffectsAsset {
    std::vector<std::string> particleLibraries;
    std::vector<std::string> stringTables;
    std::vector<std::string> soundBanks;
    std::vector<std::string> imageLists;
    std::vector<CutsceneTint> tints;
    std::vector<CutsceneSound> sounds;
    std::vector<CutsceneParticle> particles;
    std::vector<CutsceneFade> fades;
    std::vector<CutsceneText> texts;
    std::vector<CutsceneImage> images;
    std::vector<CutsceneLink> links;
};

/// Fails on features retail files don't use and on links to missing effects.
[[nodiscard]] Result<CutsceneEffectsAsset> ParseCutsceneEffects(std::span<const std::byte> bytes);

}
