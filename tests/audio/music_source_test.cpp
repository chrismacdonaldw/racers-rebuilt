#include "audio/music_source.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string_view>

#include "assets/folder_layer.hpp"
#include "assets/jam_builder.hpp"
#include "audio/alp_builder.hpp"
#include "temp_folder.hpp"

namespace rr::audio {
namespace {

constexpr std::array<std::uint8_t, 16> kPayload{};

class OriginalFixture {
public:
    OriginalFixture() : source_(vfs_) {}

    void Install(std::string_view name, std::span<const std::byte> bytes) {
        std::ofstream(folder_.Path() / name, std::ios::binary) << test::AsText(bytes);
        Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(folder_.Path());
        REQUIRE(layer.has_value());
        vfs_.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
    }

protected:
    test::TempFolder folder_;
    assets::Vfs vfs_;
    OriginalTrackSource source_;
};

TEST_CASE_FIXTURE(OriginalFixture, "the install's track opens as a stream") {
    Install("CIRCUIT1.TUN", test::BuildAlp(2, std::nullopt, kPayload));

    const Result<MusicTrack> track = source_.Open("circuit1");

    REQUIRE(track.has_value());
    CHECK(track->stream->Format().channelCount == 2);
    CHECK(track->stream->FrameCount() > 0);
}

TEST_CASE_FIXTURE(OriginalFixture, "an install track that isn't there is not found") {
    Install("CIRCUIT2.TUN", test::BuildAlp(2, std::nullopt, kPayload));

    CHECK(source_.Open("circuit1").error().code == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(OriginalFixture, "an install track that isn't ALP is broken") {
    constexpr std::array kGarbage{std::byte{'R'}, std::byte{'I'}, std::byte{'F'}, std::byte{'F'}};
    Install("CIRCUIT1.TUN", kGarbage);

    CHECK(source_.Open("circuit1").error().code == ErrorCode::InvalidData);
}

}
}
