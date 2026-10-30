#include "assets/image_list.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "archive_survey.hpp"
#include "assets/game_path.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Join;
using test::Text;

TEST_CASE("an image list reads each image's file kind and colour key") {
    const Result<std::vector<ImageListEntry>> images = ParseImageList(Join({
        Bytes({0x27}),
        test::ListHeader(2),
        Bytes({0x27}),
        Text("arrow"),
        Bytes({0x05, 0x29, 0x2B}),
        test::Ints({255, 0, 255}),
        Bytes({0x06, 0x27}),
        Text("backdrp"),
        Bytes({0x05, 0x06, 0x06}),
    }));

    REQUIRE(images.has_value());
    REQUIRE(images->size() == 2);
    CHECK(images->at(0).FileName() == "arrow.BMP");
    CHECK(images->at(0).colorKey == ColorKey{.red = 255, .blue = 255});
    CHECK(images->at(1).FileName() == "backdrp.TGA");
    CHECK_FALSE(images->at(1).colorKey.has_value());
}

std::size_t CheckRetailImageList(const JamArchive& archive, const IndexEntry& imageList) {
    CAPTURE(imageList.path);
    const Result<std::vector<ImageListEntry>> images =
        test::ParseEntry(archive, imageList, ParseImageList);
    REQUIRE(images.has_value());
    for (const ImageListEntry& image : *images) {
        const std::string path = JoinGamePath(imageList.Parent(), ToUpperAscii(image.FileName()));
        CHECK_MESSAGE(archive.Index().Find(path).has_value(), path);
    }
    return images->size();
}

TEST_CASE("every image list in the retail game parses and names images beside it") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    const std::vector<IndexEntry> imageLists = test::FilesWithExtension(*archive, "IDB");
    std::size_t imageCount = 0;
    for (const IndexEntry& imageList : imageLists) {
        imageCount += CheckRetailImageList(*archive, imageList);
    }
    CHECK(imageLists.size() == 23);
    CHECK(imageCount == 131);
}

}
}
