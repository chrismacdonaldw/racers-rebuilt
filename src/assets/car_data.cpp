#include "assets/car_data.hpp"

#include <algorithm>

namespace rr::assets {
namespace {

constexpr std::size_t kEntrySize = 8;

std::uint16_t U16At(const CarData& car, std::size_t offset) {
    return static_cast<std::uint16_t>((std::to_integer<unsigned>(car.at(offset)) << 8U) |
                                      std::to_integer<unsigned>(car.at(offset + 1)));
}

}

std::vector<std::uint16_t> PieceTypesOf(const CarData& car) {
    const std::size_t count = std::min<std::size_t>(U16At(car, 0), (car.size() - 2) / kEntrySize);
    std::vector<std::uint16_t> types;
    types.reserve(count);
    for (std::size_t entry = 0; entry < count; ++entry) {
        types.push_back(U16At(car, 2 + (entry * kEntrySize)));
    }
    return types;
}

}
