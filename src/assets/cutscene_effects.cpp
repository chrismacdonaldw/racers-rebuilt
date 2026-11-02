#include "assets/cutscene_effects.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/cutscene_tokens.hpp"
#include "assets/token_reader.hpp"
#include "assets/token_schema.hpp"
#include "core/result.hpp"

namespace rr::assets {

static Result<void> ReadValue(TokenReader& reader, CutsceneAxes& axes) {
    return ReadMembers<&CutsceneAxes::direction, &CutsceneAxes::up>(reader, axes);
}

namespace {

constexpr FieldId kParticleLibrariesSection = 0x27;
constexpr FieldId kStringTablesSection = 0x28;
constexpr FieldId kImageListsSection = 0x2A;
constexpr FieldId kSoundBanksSection = 0x5C;
constexpr FieldId kTintsSection = 0x2B;
constexpr FieldId kSoundsSection = 0x2F;
constexpr FieldId kParticlesSection = 0x3C;
constexpr FieldId kTextsSection = 0x3F;
constexpr FieldId kImagesSection = 0x4D;
constexpr FieldId kFadesSection = 0x60;
constexpr FieldId kTransformStartLinks = 0x56;
constexpr FieldId kTransformEndLinks = 0x57;
constexpr FieldId kStartMode = 0x4E;
constexpr FieldId kStopMode = 0x4F;
constexpr FieldId kFromColor = 0x63;
constexpr FieldId kToColor = 0x64;

template <auto First, auto Second>
constexpr auto IndexPairField(FieldId id) {
    using Target = MemberPointer<decltype(First)>::Target;
    return FieldRule<Target>{.id = id, .read = [](TokenReader& reader, Target& target) {
                                 return ReadIndex(reader, target.*First).and_then([&] {
                                     return ReadIndex(reader, target.*Second);
                                 });
                             }};
}

template <typename Effect>
constexpr std::array<FieldRule<Effect>, 3> EntityFields() {
    return {
        EntityNameField<&Effect::entity, true>(0x5D),
        EntityNameField<&Effect::entity, false>(0x5E),
        UnsupportedField<Effect, 0x5F>(),
    };
}

template <typename Target, std::size_t N, std::size_t M>
constexpr std::array<FieldRule<Target>, N + M> Concat(
    const std::array<FieldRule<Target>, N>& first, const std::array<FieldRule<Target>, M>& second) {
    std::array<FieldRule<Target>, N + M> rules{};
    std::ranges::copy(first, rules.begin());
    std::ranges::copy(second, rules.begin() + N);
    return rules;
}

constexpr auto kTintFields = Concat(
    std::array{
        Field<&CutsceneTint::shifts>(0x2C),
        Field<&CutsceneTint::offsets>(0x2D),
        UnsupportedField<CutsceneTint, 0x2E>(),
    },
    EntityFields<CutsceneTint>());

constexpr auto kSoundFields = Concat(
    std::array{
        IndexPairField<&CutsceneSound::bank, &CutsceneSound::sound>(0x30),
        Field<&CutsceneSound::priority>(0x31),
        Field<&CutsceneSound::volume>(0x32),
        Field<&CutsceneSound::frequencyScale>(0x33),
        Field<&CutsceneSound::pan>(0x34),
        FlagField<&CutsceneSound::isLooping, true>(0x35),
    },
    EntityFields<CutsceneSound>());

constexpr auto kParticleFields = Concat(
    std::array{
        FieldRule<CutsceneParticle>{
            .id = 0x3D,
            .read =
                [](TokenReader& reader, CutsceneParticle& particle) {
                    return ReadIndex(reader, particle.library).and_then([&] {
                        return ReadValue(reader, particle.emitter);
                    });
                }},
        Field<&CutsceneParticle::position>(0x39),
        Field<&CutsceneParticle::axes>(0x3E),
        FieldRule<CutsceneParticle>{
            .id = 0x3A, .read = [](TokenReader&, CutsceneParticle&) -> Result<void> { return {}; }},
        Field<&CutsceneParticle::joint>(0x3B),
    },
    EntityFields<CutsceneParticle>());

constexpr std::array kFadeFields{
    Field<&CutsceneFade::duration>(0x61),
    FieldRule<CutsceneFade>{
        .id = 0x62,
        .read = [](TokenReader& reader, CutsceneFade& fade) -> Result<void> {
            Result<Token> mode = reader.Expect(TokenKind::Field);
            if (!mode) {
                return std::unexpected(std::move(mode.error()));
            }
            if (mode->field != kFromColor && mode->field != kToColor) {
                return Fail(ErrorCode::InvalidData,
                            std::format("fade mode 0x{:02X} is unknown", mode->field));
            }
            fade.direction =
                mode->field == kFromColor ? FadeDirection::FromColor : FadeDirection::ToColor;
            return {};
        }},
    Field<&CutsceneFade::material>(0x65),
    Field<&CutsceneFade::color>(0x66),
};

template <typename Visual, std::optional<float> VisualLayout::* Edge>
constexpr FieldRule<Visual> CentreField(FieldId id) {
    return {.id = id, .read = [](TokenReader&, Visual& visual) -> Result<void> {
                (visual.layout.*Edge).reset();
                return {};
            }};
}

template <typename Visual>
constexpr std::array<FieldRule<Visual>, 11> VisualFields() {
    return {
        NestedField<&Visual::layout, &VisualLayout::left>(0x43),
        NestedField<&Visual::layout, &VisualLayout::top>(0x44),
        CentreField<Visual, &VisualLayout::left>(0x45),
        CentreField<Visual, &VisualLayout::top>(0x46),
        NestedField<&Visual::layout, &VisualLayout::width>(0x47),
        NestedField<&Visual::layout, &VisualLayout::height>(0x48),
        UnsupportedField<Visual, 0x49>(),
        UnsupportedField<Visual, 0x4A>(),
        UnsupportedField<Visual, 0x4B>(),
        UnsupportedField<Visual, 0x4C>(),
        NestedField<&Visual::layout, &VisualLayout::color>(0x66),
    };
}

constexpr auto kTextFields = Concat(
    std::array{
        IndexPairField<&CutsceneText::stringTable, &CutsceneText::line>(0x40),
        UnsupportedField<CutsceneText, 0x41>(),
        Field<&CutsceneText::font>(0x42),
    },
    VisualFields<CutsceneText>());

constexpr auto kImageFields =
    Concat(std::array{Field<&CutsceneImage::image>(0x4D)}, VisualFields<CutsceneImage>());

struct PendingLink {
    CutsceneLink link;
    std::string effect;
};

struct EffectsParse {
    CutsceneEffectsAsset asset;
    std::vector<PendingLink> links;
};

constexpr std::array kEffectSections{
    std::pair{kTintsSection, EffectKind::Tint},
    std::pair{kSoundsSection, EffectKind::Sound},
    std::pair{kParticlesSection, EffectKind::Particle},
    std::pair{kFadesSection, EffectKind::Fade},
    std::pair{kTextsSection, EffectKind::Text},
    std::pair{kImagesSection, EffectKind::Image},
};

Result<EffectKind> ReadLinkTarget(TokenReader& reader) {
    return reader.Expect(TokenKind::Field).and_then([](const Token& token) -> Result<EffectKind> {
        const auto section =
            std::ranges::find(kEffectSections, token.field, &std::pair<FieldId, EffectKind>::first);
        if (section == kEffectSections.end()) {
            return Fail(ErrorCode::InvalidData,
                        std::format("a link targets section 0x{:02X}", token.field));
        }
        return section->second;
    });
}

Result<bool> ReadLinkMode(TokenReader& reader) {
    return reader.Expect(TokenKind::Field).and_then([](const Token& token) -> Result<bool> {
        if (token.field != kStartMode && token.field != kStopMode) {
            return Fail(ErrorCode::InvalidData,
                        std::format("link mode 0x{:02X} is unknown", token.field));
        }
        return token.field == kStartMode;
    });
}

Result<void> ReadLinkBody(TokenReader& reader, PendingLink& pending) {
    return reader.Expect(TokenKind::OpenBrace)
        .and_then([&](const Token&) { return ReadLinkTarget(reader); })
        .and_then([&](EffectKind kind) {
            pending.link.kind = kind;
            return ReadLinkMode(reader);
        })
        .and_then([&](bool starts) {
            pending.link.starts = starts;
            return ReadName(reader);
        })
        .and_then([&](std::string effect) {
            pending.effect = std::move(effect);
            return reader.Expect(TokenKind::CloseBrace);
        })
        .transform([](const Token&) {});
}

Result<void> ReadLinks(TokenReader& reader, FieldId section, std::vector<PendingLink>& links) {
    return ReadNonEmptyList(reader, [&] {
        PendingLink& pending = links.emplace_back();
        pending.link.onTransformEnd = section == kTransformEndLinks;
        return ExpectField(reader, section)
            .and_then([&] { return ReadName(reader); })
            .and_then([&](std::string transform) {
                pending.link.transform = std::move(transform);
                return ReadLinkBody(reader, pending);
            });
    });
}

template <typename Effect>
Result<void> ReadEffects(TokenReader& reader, FieldId section,
                         std::span<const FieldRule<Effect>> rules, std::vector<Effect>& effects) {
    return ReadNamedEntries<Effect>(reader, section, rules, effects, FieldRepeats::Fail);
}

Result<void> ReadSection(TokenReader& reader, const Token& section, EffectsParse& parse) {
    CutsceneEffectsAsset& asset = parse.asset;
    switch (section.field) {
        case kParticleLibrariesSection:
            return ReadNames(reader, asset.particleLibraries);
        case kStringTablesSection:
            return ReadNames(reader, asset.stringTables);
        case kSoundBanksSection:
            return ReadNames(reader, asset.soundBanks);
        case kImageListsSection:
            return ReadNames(reader, asset.imageLists);
        case kTintsSection:
            return ReadEffects<CutsceneTint>(reader, section.field, kTintFields, asset.tints);
        case kSoundsSection:
            return ReadEffects<CutsceneSound>(reader, section.field, kSoundFields, asset.sounds);
        case kParticlesSection:
            return ReadEffects<CutsceneParticle>(reader, section.field, kParticleFields,
                                                 asset.particles);
        case kFadesSection:
            return ReadEffects<CutsceneFade>(reader, section.field, kFadeFields, asset.fades);
        case kTextsSection:
            return ReadEffects<CutsceneText>(reader, section.field, kTextFields, asset.texts);
        case kImagesSection:
            return ReadEffects<CutsceneImage>(reader, section.field, kImageFields, asset.images);
        case kTransformStartLinks:
        case kTransformEndLinks:
            return ReadLinks(reader, section.field, parse.links);
        default:
            return UnknownSection(section);
    }
}

template <typename Effect>
std::optional<std::size_t> FindEffect(const std::vector<Effect>& effects, std::string_view name) {
    const auto effect = std::ranges::find(effects, name, &Effect::name);
    if (effect == effects.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(effect - effects.begin());
}

std::optional<std::size_t> FindEffect(const CutsceneEffectsAsset& asset, EffectKind kind,
                                      std::string_view name) {
    switch (kind) {
        case EffectKind::Tint:
            return FindEffect(asset.tints, name);
        case EffectKind::Sound:
            return FindEffect(asset.sounds, name);
        case EffectKind::Particle:
            return FindEffect(asset.particles, name);
        case EffectKind::Fade:
            return FindEffect(asset.fades, name);
        case EffectKind::Text:
            return FindEffect(asset.texts, name);
        case EffectKind::Image:
            return FindEffect(asset.images, name);
    }
    return std::nullopt;
}

std::optional<std::string> EffectProblem(const CutsceneEffectsAsset& asset) {
    for (const CutsceneSound& sound : asset.sounds) {
        if (sound.bank >= asset.soundBanks.size()) {
            return std::format("sound '{}' uses a missing bank", sound.name);
        }
    }
    for (const CutsceneParticle& particle : asset.particles) {
        if (particle.library >= asset.particleLibraries.size() || particle.emitter.empty()) {
            return std::format("particle '{}' needs a library and an emitter", particle.name);
        }
    }
    for (const CutsceneText& text : asset.texts) {
        if (text.stringTable >= asset.stringTables.size() || text.font.empty()) {
            return std::format("text '{}' needs a string table and a font", text.name);
        }
    }
    for (const CutsceneImage& image : asset.images) {
        if (image.image.empty()) {
            return std::format("image '{}' names no image", image.name);
        }
    }
    return std::nullopt;
}

Result<void> Finish(EffectsParse& parse) {
    if (std::optional<std::string> problem = EffectProblem(parse.asset)) {
        return Fail(ErrorCode::InvalidData, *problem);
    }
    for (PendingLink& pending : parse.links) {
        const std::optional<std::size_t> effect =
            FindEffect(parse.asset, pending.link.kind, pending.effect);
        if (!effect) {
            return Fail(ErrorCode::InvalidData,
                        std::format("transform '{}' links to missing effect '{}'",
                                    pending.link.transform, pending.effect));
        }
        pending.link.effect = *effect;
        parse.asset.links.push_back(std::move(pending.link));
    }
    return {};
}

}

Result<CutsceneEffectsAsset> ParseCutsceneEffects(std::span<const std::byte> bytes) {
    EffectsParse parse;
    return ReadSections(bytes,
                        [&](TokenReader& reader, const Token& section) {
                            return ReadSection(reader, section, parse);
                        })
        .and_then([&] { return Finish(parse); })
        .transform([&] { return std::move(parse.asset); });
}

}
