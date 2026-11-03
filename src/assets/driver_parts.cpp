#include "assets/driver_parts.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/merged_table.hpp"
#include "assets/token_schema.hpp"
#include "core/log.hpp"

namespace rr::assets {
namespace {

constexpr std::int32_t kAllRacesUnlock = 0x80;
constexpr std::int32_t kAlwaysUnlocked = 2;
constexpr std::int32_t kFirstPartSetUnlock = 3;
constexpr std::size_t kMaxShelfParts = 256;

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

struct HeadedHat {
    std::string name;
    DriverPart hat;
    std::string headPart;
};

std::vector<HeadedHat> TakeHeadedHats(DriverPartCatalog& catalog) {
    std::vector<HeadedHat> hats;
    hats.reserve(catalog.hats.size());
    for (std::size_t i = 0; i < catalog.hats.size(); ++i) {
        hats.push_back({.name = catalog.hats[i].name,
                        .hat = std::move(catalog.hats[i]),
                        .headPart = std::move(catalog.headParts.at(i))});
    }
    return hats;
}

void PutHeadedHats(std::vector<HeadedHat> hats, DriverPartCatalog& catalog) {
    catalog.hats.clear();
    catalog.headParts.clear();
    for (HeadedHat& hat : hats) {
        catalog.hats.push_back(std::move(hat.hat));
        catalog.headParts.push_back(std::move(hat.headPart));
    }
}

template <typename Part>
std::vector<Part> Capped(std::vector<Part> parts, std::string_view shelf) {
    if (parts.size() > kMaxShelfParts) {
        log::Warn("dropping {} mod {}, a driver can pick from {} at most",
                  parts.size() - kMaxShelfParts, shelf, kMaxShelfParts);
        parts.resize(kMaxShelfParts);
    }
    return parts;
}

std::vector<DriverPart> MergeShelf(std::vector<DriverPartCatalog>& catalogs,
                                   DriverPartShelf DriverPartCatalog::* shelf,
                                   std::string_view name) {
    std::vector<std::vector<DriverPart>> parts;
    parts.reserve(catalogs.size());
    for (DriverPartCatalog& catalog : catalogs) {
        parts.push_back(std::move((catalog.*shelf).parts));
    }
    return Capped(MergeNamed(std::move(parts)), name);
}

}

DriverPartCatalog MergeDriverParts(std::vector<DriverPartCatalog> catalogs) {
    if (catalogs.empty()) {
        return {};
    }
    std::vector<DriverPart> faces = MergeShelf(catalogs, &DriverPartCatalog::faces, "faces");
    std::vector<DriverPart> torsos = MergeShelf(catalogs, &DriverPartCatalog::torsos, "torsos");
    std::vector<DriverPart> legs = MergeShelf(catalogs, &DriverPartCatalog::legs, "legs");
    std::vector<std::vector<HeadedHat>> hats;
    hats.reserve(catalogs.size());
    for (DriverPartCatalog& catalog : catalogs) {
        hats.push_back(TakeHeadedHats(catalog));
    }
    DriverPartCatalog merged = std::move(catalogs.front());
    PutHeadedHats(Capped(MergeNamed(std::move(hats)), "hats"), merged);
    merged.faces.parts = std::move(faces);
    merged.torsos.parts = std::move(torsos);
    merged.legs.parts = std::move(legs);
    return merged;
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
