#include "assets/sky_file.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <string_view>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kSkiesField = 0x2C;
constexpr FieldId kSkyField = 0x27;
constexpr FieldId kInitialField = 0x2D;
constexpr FieldId kHeightField = 0x2E;

constexpr std::array kKeyframeFields{
    Field<&SkyKeyframe::length>(0x28),
    Field<&SkyKeyframe::horizon>(0x29),
    Field<&SkyKeyframe::middle>(0x2A),
    Field<&SkyKeyframe::top>(0x2B),
};

Result<void> ReadKeyframe(TokenReader& reader, SkyGradient& sky) {
    return ExpectField(reader, kSkyField).and_then([&] {
        return ReadBlock<SkyKeyframe>(reader, kKeyframeFields, sky.keyframes.emplace_back());
    });
}

Result<void> ReadSky(TokenReader& reader, SkyGradient& sky) {
    const Result<std::size_t> count =
        ExpectField(reader, kSkyField).and_then([&] { return reader.ReadListCount(); });
    return count.and_then([&](std::size_t) { return reader.ReadString(); })
        .and_then([&](std::string_view name) {
            sky.name = name;
            return reader.Expect(TokenKind::OpenBrace);
        })
        .and_then([&](const Token&) {
            return ReadElements(reader, *count, [&] { return ReadKeyframe(reader, sky); });
        });
}

Result<bool> SkipField(TokenReader& reader, FieldId id) {
    Result<Token> next = reader.Peek();
    if (!next) {
        return std::unexpected(std::move(next.error()));
    }
    if (next->kind != TokenKind::Field || next->field != id) {
        return false;
    }
    return reader.Next().transform([](const Token&) { return true; });
}

Result<std::optional<std::string>> ReadInitial(TokenReader& reader) {
    return SkipField(reader, kInitialField)
        .and_then([&](bool isPresent) -> Result<std::optional<std::string>> {
            if (!isPresent) {
                return std::nullopt;
            }
            return reader.ReadString().transform(
                [](std::string_view name) { return std::optional<std::string>(name); });
        });
}

Result<void> ReadHeight(TokenReader& reader, float& height) {
    return SkipField(reader, kHeightField).and_then([&](bool isPresent) -> Result<void> {
        if (!isPresent) {
            return {};
        }
        return ReadValue(reader, height);
    });
}

std::size_t IndexOf(const std::vector<SkyGradient>& skies, std::string_view name) {
    const std::string wanted = ToUpperAscii(name);
    const auto found = std::ranges::find_if(
        skies, [&](const SkyGradient& sky) { return ToUpperAscii(sky.name) == wanted; });
    return found == skies.end() ? 0 : static_cast<std::size_t>(found - skies.begin());
}

Result<void> Validate(const SkyFile& file) {
    if (file.skies.empty()) {
        return Fail(ErrorCode::InvalidData, "the sky file has no skies");
    }
    for (const SkyGradient& sky : file.skies) {
        if (sky.keyframes.empty()) {
            return Fail(ErrorCode::InvalidData, std::format("sky {} has no keyframes", sky.name));
        }
        if (std::ranges::any_of(sky.keyframes, [](const SkyKeyframe& keyframe) {
                return keyframe.length < Milliseconds{0};
            })) {
            return Fail(ErrorCode::InvalidData,
                        std::format("sky {} has a negative keyframe length", sky.name));
        }
    }
    return {};
}

}

Result<SkyFile> ParseSkyFile(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    SkyFile file;
    std::optional<std::string> initial;
    return ExpectField(reader, kSkiesField)
        .and_then([&] {
            return ReadList(reader, [&] { return ReadSky(reader, file.skies.emplace_back()); });
        })
        .and_then([&] { return ReadInitial(reader); })
        .and_then([&](std::optional<std::string> name) {
            initial = std::move(name);
            return ReadHeight(reader, file.heightOffset);
        })
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .and_then([&](const Token&) { return Validate(file); })
        .transform([&] {
            file.initial = initial ? IndexOf(file.skies, *initial) : 0;
            return std::move(file);
        });
}

}
