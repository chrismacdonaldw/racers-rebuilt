#include "app/racer/car_silhouette.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <vector>

#include "app/racer/shadow_silhouette.hpp"
#include "assets/archive_survey.hpp"
#include "assets/loader_pool.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kChannels = 4;
constexpr std::size_t kMirrorSlack = 64;
constexpr float kWindowLength = 9.0F * 1.15F;
constexpr float kWindowWidth = 8.0F * 1.15F;

bool IsCovered(const assets::ImageAsset& image, std::size_t column, std::size_t row) {
    return image.rgba.at((((row * image.widthPx) + column) * kChannels) + 3) == std::byte{255};
}

std::size_t Covered(const assets::ImageAsset& image) {
    std::size_t covered = 0;
    for (std::size_t row = 0; row < kSilhouettePx; ++row) {
        for (std::size_t column = 0; column < kSilhouettePx; ++column) {
            covered += IsCovered(image, column, row) ? 1 : 0;
        }
    }
    return covered;
}

std::size_t Unmirrored(const assets::ImageAsset& image) {
    std::size_t unmatched = 0;
    for (std::size_t row = 0; row < kSilhouettePx; ++row) {
        for (std::size_t column = 0; column < kSilhouettePx; ++column) {
            const bool mirrored = IsCovered(image, kSilhouettePx - 1 - column, row);
            unmatched += IsCovered(image, column, row) != mirrored ? 1 : 0;
        }
    }
    return unmatched;
}

TEST_CASE("a retail car's silhouette covers the car and mirrors across it") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    const Result<std::vector<Vector3>> corners =
        assets::RacerCatalog::Load(*vfs)
            .and_then([&](const assets::RacerCatalog& catalog) {
                return LoadRacer(*vfs, loader, catalog, "RR");
            })
            .and_then([](const RacerAssets& racer) { return SilhouetteCorners(racer); });
    REQUIRE(corners.has_value());
    REQUIRE(corners->size() % 3 == 0);

    const assets::ImageAsset image = ShadowSilhouette(*corners, kWindowLength, kWindowWidth);

    CHECK(Covered(image) > kSilhouettePx * kSilhouettePx / 4);
    CHECK(Unmirrored(image) < kMirrorSlack);
}

}
}
