#include "assets/champion_database.hpp"

#include <array>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kChampionField = 0x27;

constexpr std::array kChampionFields{
    NestedField<&ChampionEntry::body, &ModelFiles::materials>(0x28),
    NestedField<&ChampionEntry::body, &ModelFiles::textures>(0x29),
    NestedField<&ChampionEntry::body, &ModelFiles::geometry>(0x2A),
    Field<&ChampionEntry::chassis>(0x2B),
    Field<&ChampionEntry::mass>(0x2C),
    Field<&ChampionEntry::centreOfMass>(0x2D),
};

}

Result<std::vector<ChampionEntry>> ParseChampions(std::span<const std::byte> bytes) {
    return ParseNamedLibrary<ChampionEntry>(bytes, kChampionField, kChampionFields);
}

}
