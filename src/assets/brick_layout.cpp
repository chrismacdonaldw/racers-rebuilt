#include "assets/brick_layout.hpp"

#include <array>
#include <format>
#include <optional>
#include <string>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kColourField = 0x27;
constexpr FieldId kWhiteField = 0x2F;

struct StoredBrick {
    std::optional<Vector3> position;
    BrickColour colour = BrickColour::Green;
    std::optional<std::int32_t> respawnMs;
};

constexpr std::array kBrickFields{
    Field<&StoredBrick::position>(0x28),
    FlagField<&StoredBrick::colour, BrickColour::Red>(0x2A),
    FlagField<&StoredBrick::colour, BrickColour::Yellow>(0x2B),
    FlagField<&StoredBrick::colour, BrickColour::Blue>(0x2C),
    FlagField<&StoredBrick::colour, BrickColour::Green>(0x2D),
    Field<&StoredBrick::respawnMs>(0x2E),
    IgnoredField<StoredBrick, std::string, std::int32_t>(0x30),
    IgnoredField<StoredBrick, std::int32_t>(0x31),
};

struct StoredWhiteBrick {
    std::optional<Vector3> position;
};

constexpr std::array kWhiteBrickFields{
    Field<&StoredWhiteBrick::position>(0x28),
};

template <typename Stored>
Result<Stored> ReadBrick(TokenReader& reader, FieldId field,
                         std::span<const FieldRule<Stored>> rules) {
    Stored stored;
    return ExpectField(reader, field)
        .and_then([&] { return ReadBlock<Stored>(reader, rules, stored); })
        .and_then([&]() -> Result<Stored> {
            if (!stored.position) {
                return Fail(ErrorCode::InvalidData, "a brick is missing its position");
            }
            return stored;
        });
}

Result<void> ReadColourBricks(TokenReader& reader, std::vector<ColourBrickPlacement>& bricks) {
    return ExpectField(reader, kColourField).and_then([&] {
        return ReadList(reader, [&] {
            return ReadBrick<StoredBrick>(reader, kColourField, kBrickFields)
                .transform([&](const StoredBrick& stored) {
                    bricks.push_back({.position = *stored.position,
                                      .colour = stored.colour,
                                      .respawn = stored.respawnMs ? Milliseconds{*stored.respawnMs}
                                                                  : kDefaultBrickRespawn});
                });
        });
    });
}

Result<void> ReadWhiteBricks(TokenReader& reader, std::vector<Vector3>& bricks) {
    return ExpectField(reader, kWhiteField).and_then([&] {
        return ReadList(reader, [&] {
            return ReadBrick<StoredWhiteBrick>(reader, kWhiteField, kWhiteBrickFields)
                .transform(
                    [&](const StoredWhiteBrick& stored) { bricks.push_back(*stored.position); });
        });
    });
}

}

Result<BrickLayout> ParsePwb(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    BrickLayout layout;
    return ReadColourBricks(reader, layout.coloured)
        .and_then([&] { return ReadWhiteBricks(reader, layout.white); })
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .transform([&](const Token&) { return std::move(layout); });
}

}
