#include "assets/ldraw_extra_pieces.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <ranges>
#include <string>

#include "assets/game_path.hpp"

namespace rr::assets {
namespace {

constexpr std::uint8_t kOccupied = 0x80;
constexpr std::uint8_t kRests = 0x80;
constexpr std::uint8_t kSitsLevel = 0x40;

struct ExtraPiece {
    std::uint16_t type = 0;
    std::string_view name;
    std::string_view file;
    Vector3 offset;
    std::string_view cells;
};

constexpr ExtraPiece Extra(std::uint16_t type, std::string_view name, std::string_view file,
                           Vector3 offset, std::string_view cells) {
    return {.type = type, .name = name, .file = file, .offset = offset, .cells = cells};
}

constexpr std::array kExtraPieces{
    Extra(0x3000, "X3005", "3005.dat", {.x = 0.5F, .y = 0.5F, .z = 1.2F}, "1x1 3o"),
    Extra(0x3001, "X3009", "3009.dat", {.x = 3.0F, .y = 0.5F, .z = 1.2F}, "6x1 3o"),
    Extra(0x3002, "X3008", "3008.dat", {.x = 4.0F, .y = 0.5F, .z = 1.2F}, "8x1 3o"),
    Extra(0x3003, "X6111", "6111.dat", {.x = 5.0F, .y = 0.5F, .z = 1.2F}, "10x1 3o"),
    Extra(0x3004, "X2456", "2456.dat", {.x = 3.0F, .y = 1.0F, .z = 1.2F}, "6x2 3o"),
    Extra(0x3005, "X3007", "3007.dat", {.x = 4.0F, .y = 1.0F, .z = 1.2F}, "8x2 3o"),
    Extra(0x3006, "X3006", "3006.dat", {.x = 5.0F, .y = 1.0F, .z = 1.2F}, "10x2 3o"),
    Extra(0x3007, "X2357", "2357.dat", {.x = 0.5F, .y = 0.5F, .z = 1.2F}, "2x2 3o 3o / 3o ."),
    Extra(0x3008, "X3245C", "3245c.dat", {.x = 1.0F, .y = 0.5F, .z = 2.4F}, "2x1 6o"),
    Extra(0x3009, "X3024", "3024.dat", {.x = 0.5F, .y = 0.5F, .z = 0.4F}, "1x1 1o"),
    Extra(0x300A, "X3023B", "3023b.dat", {.x = 1.0F, .y = 0.5F, .z = 0.4F}, "2x1 1o"),
    Extra(0x300B, "X3623", "3623.dat", {.x = 1.5F, .y = 0.5F, .z = 0.4F}, "3x1 1o"),
    Extra(0x300C, "X3710", "3710.dat", {.x = 2.0F, .y = 0.5F, .z = 0.4F}, "4x1 1o"),
    Extra(0x300D, "X3666", "3666.dat", {.x = 3.0F, .y = 0.5F, .z = 0.4F}, "6x1 1o"),
    Extra(0x300E, "X3460", "3460.dat", {.x = 4.0F, .y = 0.5F, .z = 0.4F}, "8x1 1o"),
    Extra(0x300F, "X4477", "4477.dat", {.x = 5.0F, .y = 0.5F, .z = 0.4F}, "10x1 1o"),
    Extra(0x3010, "X3022", "3022.dat", {.x = 1.0F, .y = 1.0F, .z = 0.4F}, "2x2 1o"),
    Extra(0x3011, "X3021", "3021.dat", {.x = 1.5F, .y = 1.0F, .z = 0.4F}, "3x2 1o"),
    Extra(0x3012, "X3020", "3020.dat", {.x = 2.0F, .y = 1.0F, .z = 0.4F}, "4x2 1o"),
    Extra(0x3013, "X3795", "3795.dat", {.x = 3.0F, .y = 1.0F, .z = 0.4F}, "6x2 1o"),
    Extra(0x3014, "X3034", "3034.dat", {.x = 4.0F, .y = 1.0F, .z = 0.4F}, "8x2 1o"),
    Extra(0x3015, "X3832", "3832.dat", {.x = 5.0F, .y = 1.0F, .z = 0.4F}, "10x2 1o"),
    Extra(0x3016, "X3031", "3031.dat", {.x = 2.0F, .y = 2.0F, .z = 0.4F}, "4x4 1o"),
    Extra(0x3017, "X3032", "3032.dat", {.x = 3.0F, .y = 2.0F, .z = 0.4F}, "6x4 1o"),
    Extra(0x3018, "X3035", "3035.dat", {.x = 4.0F, .y = 2.0F, .z = 0.4F}, "8x4 1o"),
    Extra(0x3019, "X3030", "3030.dat", {.x = 5.0F, .y = 2.0F, .z = 0.4F}, "10x4 1o"),
    Extra(0x301A, "X3958", "3958.dat", {.x = 3.0F, .y = 3.0F, .z = 0.4F}, "6x6 1o"),
    Extra(0x301B, "X3036", "3036.dat", {.x = 4.0F, .y = 3.0F, .z = 0.4F}, "8x6 1o"),
    Extra(0x301C, "X3033", "3033.dat", {.x = 5.0F, .y = 3.0F, .z = 0.4F}, "10x6 1o"),
    Extra(0x301D, "X2420", "2420.dat", {.x = 0.5F, .y = 0.5F, .z = 0.4F}, "2x2 1o 1o / 1o ."),
    Extra(0x301E, "X2450", "2450.dat", {.x = 1.5F, .y = 1.5F, .z = 0.4F},
          "3x3 1o 1s . / 1o 1o 1s / 1o 1o 1o"),
    Extra(0x301F, "X3070B", "3070b.dat", {.x = 0.5F, .y = 0.5F, .z = 0.4F}, "1x1 1"),
    Extra(0x3020, "X3069B", "3069b.dat", {.x = 1.0F, .y = 0.5F, .z = 0.4F}, "2x1 1"),
    Extra(0x3021, "X63864", "63864.dat", {.x = 1.5F, .y = 0.5F, .z = 0.4F}, "3x1 1"),
    Extra(0x3022, "X2431", "2431.dat", {.x = 2.0F, .y = 0.5F, .z = 0.4F}, "4x1 1"),
    Extra(0x3023, "X6636", "6636.dat", {.x = 3.0F, .y = 0.5F, .z = 0.4F}, "6x1 1"),
    Extra(0x3024, "X4162", "4162.dat", {.x = 4.0F, .y = 0.5F, .z = 0.4F}, "8x1 1"),
    Extra(0x3025, "X3068B", "3068b.dat", {.x = 1.0F, .y = 1.0F, .z = 0.4F}, "2x2 1"),
    Extra(0x3026, "X26603", "26603.dat", {.x = 1.5F, .y = 1.0F, .z = 0.4F}, "3x2 1"),
    Extra(0x3027, "X87079", "87079.dat", {.x = 2.0F, .y = 1.0F, .z = 0.4F}, "4x2 1"),
    Extra(0x3028, "X3037", "3037.dat", {.x = 2.0F, .y = 1.5F, .z = 1.2F},
          "4x2 3 3 3 3 / 3o 3o 3o 3o"),
    Extra(0x3029, "X3043", "3043.dat", {.x = 1.0F, .y = 1.0F, .z = 1.2F}, "2x2 3"),
    Extra(0x302A, "X3044B", "3044b.dat", {.x = 0.5F, .y = 1.0F, .z = 1.2F}, "1x2 3"),
    Extra(0x302B, "X3048B", "3048b.dat", {.x = 1.0F, .y = 0.5F, .z = 1.2F}, "2x1 3"),
    Extra(0x302C, "X4445", "4445.dat", {.x = 4.0F, .y = 1.5F, .z = 1.2F},
          "8x2 3 3 3 3 3 3 3 3 / 3o 3o 3o 3o 3o 3o 3o 3o"),
    Extra(0x302D, "X4460B", "4460b.dat", {.x = 0.5F, .y = 1.5F, .z = 3.6F}, "1x2 9 / 9o"),
    Extra(0x302E, "X60481A", "60481a.dat", {.x = 0.5F, .y = 1.5F, .z = 2.4F}, "1x2 6 / 6o"),
    Extra(0x302F, "X54200", "54200.dat", {.x = 0.5F, .y = 0.5F, .z = 0.0F}, "1x1 2"),
    Extra(0x3030, "X85984", "85984.dat", {.x = 1.0F, .y = 0.5F, .z = 0.0F}, "2x1 2"),
    Extra(0x3031, "X11477", "11477.dat", {.x = 0.5F, .y = 1.0F, .z = 0.0F}, "1x2 2 / 2^1"),
    Extra(0x3032, "X50950", "50950.dat", {.x = 0.5F, .y = 1.5F, .z = 1.2F}, "1x3 2 / 3 / 3^1"),
    Extra(0x3033, "X15068", "15068.dat", {.x = 1.0F, .y = 1.0F, .z = 0.0F}, "2x2 2 2 / 2^1 2^1"),
    Extra(0x3034, "X30363", "30363.dat", {.x = 1.0F, .y = 3.5F, .z = 1.2F},
          "2x4 2 2 / 3 3 / 3 3 / 3o 3o"),
    Extra(0x3035, "X41769A", "41769a.dat", {.x = 1.0F, .y = 2.0F, .z = 0.4F},
          "2x4 1s 1o / 1s 1o / 1s 1o / 1 1o"),
    Extra(0x3036, "X41770A", "41770a.dat", {.x = 1.0F, .y = 2.0F, .z = 0.4F},
          "2x4 1o 1s / 1o 1s / 1o 1s / 1o 1"),
    Extra(0x3037, "X43722A", "43722a.dat", {.x = 1.0F, .y = 1.5F, .z = 0.4F},
          "2x3 1s 1o / 1s 1o / 1s 1o"),
    Extra(0x3038, "X43723A", "43723a.dat", {.x = 1.0F, .y = 1.5F, .z = 0.4F},
          "2x3 1o 1s / 1o 1s / 1o 1s"),
    Extra(0x3039, "X41747", "41747.dat", {.x = 1.5F, .y = 5.0F, .z = 1.2F},
          "2x6 2s 2 / 2s 3 / 3s 3 / 3 3 / 3 3o / 3 3o"),
    Extra(0x303A, "X41748", "41748.dat", {.x = 0.5F, .y = 5.0F, .z = 1.2F},
          "2x6 2 2s / 3 2s / 3 3s / 3 3 / 3o 3 / 3o 3"),
    Extra(0x303B, "X51739", "51739.dat", {.x = 2.0F, .y = 1.0F, .z = 0.4F},
          "4x2 1s 1o 1o 1s / 1s 1o 1o 1s"),
    Extra(0x303C, "X30503", "30503.dat", {.x = 2.0F, .y = 2.0F, .z = 0.4F},
          "4x4 1o 1s . . / 1o 1o 1s . / 1o 1o 1o 1s / 1o 1o 1o 1o"),
    Extra(0x303D, "X41767", "41767.dat", {.x = 1.0F, .y = 2.0F, .z = 1.2F},
          "2x4 3s 3o / 3s 3o / 3s 3o / 3 3o"),
    Extra(0x303E, "X41768", "41768.dat", {.x = 1.0F, .y = 2.0F, .z = 1.2F},
          "2x4 3o 3s / 3o 3s / 3o 3s / 3o 3"),
    Extra(0x303F, "X3935", "3935.dat", {.x = 2.0F, .y = 2.0F, .z = 0.4F},
          "4x4 . . 1s 1o / . 1s 1o 1o / 1s 1o 1o 1o / . 1o 1o 1o"),
    Extra(0x3040, "X3936", "3936.dat", {.x = 2.0F, .y = 2.0F, .z = 0.4F},
          "4x4 1o 1s . . / 1o 1o 1s . / 1o 1o 1o 1s / 1o 1o 1o ."),
    Extra(0x3041, "X24299", "24299.dat", {.x = 1.0F, .y = 1.0F, .z = 0.4F}, "2x2 1o 1s / 1o 1s"),
    Extra(0x3042, "X24307", "24307.dat", {.x = 1.0F, .y = 1.0F, .z = 0.4F}, "2x2 1s 1o / 1s 1o"),
};

constexpr auto kExtraParts = [] {
    std::array<LdrawPart, kExtraPieces.size()> parts{};
    std::ranges::transform(kExtraPieces, parts.begin(), [](const ExtraPiece& extra) {
        return LdrawPart{.piece = extra.name,
                         .file = extra.file,
                         .matrix = {.position = extra.offset},
                         .colour = {}};
    });
    return parts;
}();

int TakeNumber(std::string_view& text) {
    int value = 0;
    const std::from_chars_result read =
        std::from_chars(text.data(), text.data() + text.size(), value);
    text.remove_prefix(static_cast<std::size_t>(read.ptr - text.data()));
    return value;
}

ShapeCell CellOf(std::string_view token) {
    if (token == ".") {
        return {};
    }
    const int top = TakeNumber(token);
    const bool isOccupied = token.starts_with('o');
    token.remove_prefix(isOccupied ? 1 : 0);
    int bottom = 0;
    if (token.starts_with('^')) {
        token.remove_prefix(1);
        bottom = TakeNumber(token);
    }
    std::uint8_t flags = 0;
    if (bottom == 0) {
        flags = token == "s" ? kSitsLevel : kRests;
    }
    return {.top = static_cast<std::uint8_t>((isOccupied ? kOccupied : 0U) | top),
            .bottom = static_cast<std::uint8_t>(flags | bottom)};
}

std::vector<std::string_view> CellWords(std::string_view text) {
    std::vector<std::string_view> words;
    for (const auto word : std::views::split(text, ' ')) {
        const std::string_view cell(word.begin(), word.end());
        if (!cell.empty() && cell != "/") {
            words.push_back(cell);
        }
    }
    return words;
}

PieceShape ShapeOf(std::string_view cells) {
    PieceShape shape;
    shape.size.width = TakeNumber(cells);
    cells.remove_prefix(1);
    shape.size.height = TakeNumber(cells);
    const std::vector<std::string_view> words = CellWords(cells);
    const auto width = static_cast<std::size_t>(shape.size.width);
    const auto depth = static_cast<std::size_t>(shape.size.height);
    shape.cells.resize(width * depth, words.empty() ? ShapeCell{} : CellOf(words.front()));
    if (words.size() == shape.cells.size()) {
        for (std::size_t word = 0; word < words.size(); ++word) {
            shape.cells[((word % width) * depth) + (word / width)] = CellOf(words[word]);
        }
    }
    return shape;
}

}

std::span<const LdrawPart> ExtraPieceParts() {
    return kExtraParts;
}

std::vector<PieceRecord> ExtraPieceRecords() {
    std::vector<PieceRecord> records;
    records.reserve(kExtraPieces.size());
    for (const ExtraPiece& extra : kExtraPieces) {
        records.push_back(
            {.name = std::string(extra.name), .type = extra.type, .shape = ShapeOf(extra.cells)});
    }
    return records;
}

bool IsExtraPiece(std::string_view pieceName) {
    const std::string wanted = ToUpperAscii(pieceName);
    return std::ranges::any_of(kExtraPieces,
                               [&](const ExtraPiece& extra) { return extra.name == wanted; });
}

}
