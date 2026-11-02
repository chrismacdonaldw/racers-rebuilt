#include "assets/driver_parts.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <iterator>
#include <string>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr std::int32_t kAllRacesUnlock = 0x80;
constexpr std::int32_t kAlwaysUnlocked = 2;
constexpr std::int32_t kFirstPartSetUnlock = 3;

constexpr std::array<std::string_view, 6> kExpressionSuffixes{"dflt",  "angry", "blink",
                                                              "happy", "sad",   "suprz"};

template <auto... Members>
Result<void> ReadParts(TokenReader& reader, std::size_t count, std::vector<DriverPart>& parts) {
    parts.clear();
    return ReadElements(reader, count,
                        [&] { return ReadMembers<Members...>(reader, parts.emplace_back()); });
}

Result<void> ReadHats(TokenReader& reader, DriverPartCatalog& catalog) {
    return reader.ReadListHeader().and_then([&](std::size_t count) {
        return ReadParts<&DriverPart::name, &DriverPart::unlock>(reader, count, catalog.hats);
    });
}

template <auto... Members>
Result<void> ReadShelf(TokenReader& reader, DriverPartShelf& shelf, bool hasHooks) {
    Result<std::size_t> count = reader.ReadListHeader();
    Result<void> read =
        count.and_then([&](std::size_t) { return ReadValue(reader, shelf.folder); });
    if (hasHooks) {
        read = read.and_then([&] { return ReadValue(reader, shelf.hookFolder); });
    }
    return read.and_then([&] { return ReadParts<Members...>(reader, *count, shelf.parts); });
}

Result<void> ReadFaces(TokenReader& reader, DriverPartCatalog& catalog) {
    return ReadShelf<&DriverPart::name, &DriverPart::unlock>(reader, catalog.faces, false);
}

template <auto Shelf>
Result<void> ReadDressShelf(TokenReader& reader, DriverPartCatalog& catalog) {
    return ReadShelf<&DriverPart::name, &DriverPart::variant, &DriverPart::unlock>(
        reader, catalog.*Shelf, true);
}

constexpr std::array kCatalogFields{
    Field<&DriverPartCatalog::modelFiles>(0x27),
    Field<&DriverPartCatalog::modelFolders>(0x28),
    FieldRule<DriverPartCatalog>{.id = 0x29, .read = ReadHats},
    FieldRule<DriverPartCatalog>{.id = 0x2A, .read = ReadFaces},
    FieldRule<DriverPartCatalog>{.id = 0x2B, .read = ReadDressShelf<&DriverPartCatalog::torsos>},
    FieldRule<DriverPartCatalog>{.id = 0x2C, .read = ReadDressShelf<&DriverPartCatalog::legs>},
    Field<&DriverPartCatalog::headParts>(0x2D),
    Field<&DriverPartCatalog::bodies>(0x2E),
    Field<&DriverPartCatalog::slots>(0x2F),
};

Result<void> CheckHeadParts(const DriverPartCatalog& catalog) {
    if (catalog.headParts.size() != catalog.hats.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} hats have {} head parts", catalog.hats.size(),
                                catalog.headParts.size()));
    }
    return {};
}

}

Result<DriverPartCatalog> ParseDriverParts(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    DriverPartCatalog catalog;
    return ReadFields<DriverPartCatalog>(reader, kCatalogFields, catalog, FieldRepeats::Fail)
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .and_then([&](const Token&) { return CheckHeadParts(catalog); })
        .transform([&] { return std::move(catalog); });
}

bool IsPartUnlocked(std::int32_t unlock, const std::set<std::int32_t>& partSets,
                    bool allRacesUnlocked) {
    if (unlock == kAllRacesUnlock) {
        return allRacesUnlocked;
    }
    return unlock <= kAlwaysUnlocked || partSets.contains(unlock - kFirstPartSetUnlock);
}

std::string FaceMaterial(std::string_view prefix, std::int32_t expression) {
    const bool isKnown = expression >= 0 && std::cmp_less(expression, kExpressionSuffixes.size());
    const std::string_view suffix =
        isKnown ? kExpressionSuffixes.at(static_cast<std::size_t>(expression))
                : kExpressionSuffixes.front();
    return std::format("{}{}", prefix, suffix);
}

std::int32_t FaceExpression(std::string_view suffix) {
    const std::string upper = ToUpperAscii(suffix);
    const auto found = std::ranges::find_if(
        kExpressionSuffixes, [&](std::string_view known) { return ToUpperAscii(known) == upper; });
    return found == kExpressionSuffixes.end()
               ? 0
               : static_cast<std::int32_t>(std::distance(kExpressionSuffixes.begin(), found));
}

}
