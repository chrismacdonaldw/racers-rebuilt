#include "assets/merged_table.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/folder_layer.hpp"
#include "temp_folder.hpp"

namespace rr::assets {
namespace {

struct Line {
    std::string name;
    std::string value;

    friend bool operator==(const Line&, const Line&) = default;
};

Line Named(std::string_view name, std::string_view value) {
    return {.name = std::string(name), .value = std::string(value)};
}

Result<std::vector<Line>> ParseLines(std::span<const std::byte> bytes) {
    const std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (text.starts_with('!')) {
        return Fail(ErrorCode::InvalidData, "broken table");
    }
    std::vector<Line> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        const std::string_view line = std::string_view(text).substr(start, end - start);
        const std::size_t split = line.find('=');
        lines.push_back({.name = std::string(line.substr(0, split)),
                         .value = std::string(line.substr(split + 1))});
        start = end + 1;
    }
    return lines;
}

class LayersFixture {
public:
    void AddLayer(std::string_view table) {
        const std::filesystem::path root =
            folder_.Path() / std::to_string(layerCount_++) / "MENUDATA";
        std::filesystem::create_directories(root);
        std::ofstream(root / "TABLE.TXT", std::ios::binary) << table;
        Result<FolderLayer> layer = FolderLayer::Scan(root.parent_path());
        REQUIRE(layer.has_value());
        vfs_.Mount(std::make_unique<FolderLayer>(std::move(*layer)));
    }

    [[nodiscard]] Result<std::vector<Line>> Read() const {
        return ReadMergedTable(vfs_, "MENUDATA/TABLE.TXT", ParseLines);
    }

private:
    test::TempFolder folder_;
    Vfs vfs_;
    int layerCount_ = 0;
};

TEST_CASE("a later table replaces a same-named entry in place and appends new ones") {
    const std::vector<Line> merged = MergeNamed<Line>({{Named("a", "1"), Named("b", "2")},
                                                       {Named("c", "3"), Named("B", "4")},
                                                       {Named("d", "5")}});

    CHECK(merged ==
          std::vector<Line>{Named("a", "1"), Named("B", "4"), Named("c", "3"), Named("d", "5")});
}

TEST_CASE("one table is kept as it is, duplicates included") {
    CHECK(MergeNamed<Line>({{Named("a", "1"), Named("A", "2")}}) ==
          std::vector<Line>{Named("a", "1"), Named("A", "2")});
    CHECK(MergeNamed<Line>({}).empty());
}

TEST_CASE("a table read from every layer merges them lowest first") {
    LayersFixture layers;
    layers.AddLayer("a=game\nb=game");
    layers.AddLayer("b=mod one\nc=mod one");
    layers.AddLayer("c=mod two");

    const Result<std::vector<Line>> table = layers.Read();

    REQUIRE(table.has_value());
    CHECK(*table ==
          std::vector<Line>{Named("a", "game"), Named("b", "mod one"), Named("c", "mod two")});
}

TEST_CASE("a missing table or a broken layer fails") {
    LayersFixture layers;
    CHECK(layers.Read().error().code == ErrorCode::NotFound);

    layers.AddLayer("a=game");
    layers.AddLayer("!");

    CHECK(layers.Read().error().code == ErrorCode::InvalidData);
}

}
}
