#include <doctest/doctest.h>

#include <array>
#include <filesystem>
#include <string_view>

#include "assets/jam_archive.hpp"
#include "audio/alp.hpp"
#include "core/utf8_path.hpp"
#include "game_files.hpp"
#include "platform/mapped_file.hpp"

namespace rr::audio {
namespace {

void DecodeWhole(std::span<const std::byte> bytes, std::string_view name) {
    CAPTURE(name);
    const Result<AlpSound> sound = ParseAlp(bytes);
    REQUIRE(sound.has_value());
    AlpDecoder decoder(sound.value_or(AlpSound{}));
    std::array<std::int16_t, 4096> samples{};
    std::uint64_t frames = 0;
    for (std::uint64_t read = 1; read > 0; frames += read) {
        read = decoder.Read(samples);
    }
    CHECK(frames > 0);
    CHECK(frames == decoder.FrameCount());
}

int DecodeEffects(const assets::JamArchive& jam) {
    int effectCount = 0;
    for (const assets::IndexEntry& entry : jam.Index().Entries()) {
        if (entry.info.kind != assets::EntryKind::File || !entry.path.ends_with(".PCM")) {
            continue;
        }
        Result<assets::FileData> file = jam.Read(entry.info);
        REQUIRE(file.has_value());
        DecodeWhole(file->Bytes(), entry.path);
        ++effectCount;
    }
    return effectCount;
}

TEST_CASE("every .PCM effect in LEGO.JAM decodes") {
    const std::optional<std::filesystem::path> folder = test::GameFolder();
    if (!folder) {
        return;
    }
    Result<assets::JamArchive> jam = assets::JamArchive::Open(*folder / "LEGO.JAM");
    REQUIRE(jam.has_value());

    CHECK(DecodeEffects(*jam) == 730);
}

TEST_CASE("every .tun music track in the install folder decodes") {
    const std::optional<std::filesystem::path> folder = test::GameFolder();
    if (!folder) {
        return;
    }
    int trackCount = 0;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(*folder)) {
        if (entry.path().extension() == ".tun") {
            Result<platform::MappedFile> file = platform::MappedFile::Open(entry.path());
            REQUIRE(file.has_value());
            DecodeWhole(file->Bytes(), Utf8FromPath(entry.path().filename()));
            ++trackCount;
        }
    }
    CHECK(trackCount == 50);
}

}
}
