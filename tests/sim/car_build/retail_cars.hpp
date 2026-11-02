#pragma once

#include <doctest/doctest.h>

#include <optional>
#include <utility>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/original_save.hpp"
#include "assets/piece_catalog.hpp"

namespace rr::sim::test {

struct RetailCars {
    assets::PieceCatalog catalog;
    std::vector<assets::SavedRacer> racers;
};

inline std::optional<RetailCars> LoadRetailCars(assets::PieceDetail detail) {
    const std::optional<assets::Vfs> vfs = rr::test::OpenRetailVfs();
    if (!vfs) {
        return std::nullopt;
    }
    Result<assets::PieceCatalog> catalog = assets::PieceCatalog::Load(*vfs, detail);
    REQUIRE(catalog.has_value());
    RetailCars cars{.catalog = std::move(*catalog), .racers = {}};
    for (const char* path : {"MENUDATA/DEFAULT.LRS", "MENUDATA/QBUILD.LRS"}) {
        const Result<assets::OriginalSave> file = vfs->Read(path).and_then(
            [](const assets::FileData& data) { return assets::ParseOriginalSave(data.Bytes()); });
        REQUIRE(file.has_value());
        cars.racers.insert(cars.racers.end(), file->racers.begin(), file->racers.end());
    }
    return cars;
}

}
