#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <vector>

#include "sim/car_build/built_car.hpp"
#include "sim/car_build/car_pieces.hpp"
#include "sim/car_build/retail_cars.hpp"

namespace rr::sim {
namespace {

assets::PieceLibrary Library() {
    return {.pieces = {test::Chassis(), test::Block(0x900, 1, 1, 3), test::Block(0x901, 2, 1, 3)}};
}

std::vector<std::byte> Bytes(std::initializer_list<unsigned> values) {
    std::vector<std::byte> bytes;
    for (const unsigned value : values) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

TEST_CASE("a car saves its pieces big-endian and loads back the same") {
    const assets::PieceLibrary library = Library();
    BuiltCar build;
    REQUIRE(build.Place(library.pieces[0], {}, 4, 3).has_value());
    REQUIRE(
        build.Place(library.pieces[2], {.x = 8, .y = 2, .rotation = 1}, 12, 0x0102).has_value());

    const assets::CarData saved = build.Save();
    const Result<BuiltCar> loaded = BuiltCar::Load(saved, library);

    CHECK(std::vector(saved.begin(), saved.begin() + 18) ==
          Bytes({0, 2, 0, 17, 0, 0, 0, 4, 0, 3, 0x09, 0x01, 8, 2, 1, 12, 1, 2}));
    REQUIRE(loaded.has_value());
    CHECK(loaded->Save() == saved);
    REQUIRE(loaded->Pieces().size() == 2);
    CHECK(loaded->Pieces()[1].height == 1);
}

TEST_CASE("a car with over 64 pieces or cut short doesn't load") {
    const assets::PieceLibrary library = Library();

    CHECK_FALSE(BuiltCar::Load(Bytes({0, 65}), library).has_value());
    CHECK_FALSE(BuiltCar::Load(Bytes({0, 1, 0, 17, 0, 0, 0}), library).has_value());
    CHECK_FALSE(BuiltCar::Load(Bytes({0}), library).has_value());
}

TEST_CASE("loading skips unknown pieces and pieces past the height limit") {
    const assets::PieceLibrary library = Library();
    std::vector<std::byte> data = Bytes({0, 7, 0, 17, 0, 0, 0, 0, 0, 0, 0x0A, 0, 1, 1, 0, 0, 0, 0});
    for (int brick = 0; brick < 5; ++brick) {
        const std::vector<std::byte> entry = Bytes({0x09, 0, 3, 3, 0, 0, 0, 0});
        data.insert(data.end(), entry.begin(), entry.end());
    }

    const Result<BuiltCar> loaded = BuiltCar::Load(data, library);

    REQUIRE(loaded.has_value());
    CHECK(loaded->Pieces().size() == 5);
    CHECK(loaded->Grid().At(3, 3).height == 13);
}

std::size_t PieceCount(const assets::CarData& car) {
    return (std::to_integer<std::size_t>(car[0]) << 8U) | std::to_integer<std::size_t>(car[1]);
}

bool LoadsWhole(const assets::SavedRacer& racer, const assets::PieceLibrary& library) {
    const Result<BuiltCar> build = BuiltCar::Load(racer.car, library);
    return build && build->Pieces().size() == PieceCount(racer.car) &&
           build->Pieces().front().piece->IsChassis() && build->Save() == racer.car;
}

std::optional<BrickVolume> JoanBricks() {
    const std::optional<test::RetailCars> cars = test::LoadRetailCars(assets::PieceDetail::Menu);
    if (!cars) {
        return std::nullopt;
    }
    REQUIRE(cars->racers.front().name == u"JOAN OF KART");
    const Result<BuiltCar> joan = BuiltCar::Load(cars->racers.front().car, cars->catalog.library);
    REQUIRE(joan.has_value());
    return joan->Bricks();
}

TEST_CASE("every retail car loads whole and saves back byte for byte") {
    const std::optional<test::RetailCars> cars = test::LoadRetailCars(assets::PieceDetail::Menu);
    if (!cars) {
        return;
    }

    CHECK(cars->racers.size() == 28);
    CHECK(std::ranges::all_of(cars->racers, [&](const assets::SavedRacer& racer) {
        return LoadsWhole(racer, cars->catalog.library);
    }));
}

TEST_CASE("JOAN OF KART's bricks match the original's count and centroid") {
    const std::optional<BrickVolume> bricks = JoanBricks();
    if (!bricks) {
        return;
    }

    CHECK(bricks->cells == 214);
    CHECK(bricks->centroid.x == doctest::Approx((788.0F / 214.0F) - 4.5F));
    CHECK(bricks->centroid.y == doctest::Approx((515.0F / 214.0F) - 2.5F));
    CHECK(bricks->centroid.z == doctest::Approx(1380.0F / 214.0F));
}

}
}
