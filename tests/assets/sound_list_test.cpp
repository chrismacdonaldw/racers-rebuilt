#include "assets/sound_list.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "assets/jam_archive.hpp"

namespace rr::assets {
namespace {

std::vector<std::byte> Text(std::string_view text) {
    const auto* first = reinterpret_cast<const std::byte*>(text.data());
    return {first, first + text.size()};
}

TEST_CASE("a sound list reads its folder and names in order") {
    const Result<SoundList> list = ParseSoundList(Text("Sounds\\\r\n2\r\nengine.pcm\r\nIdle.pcm"));

    REQUIRE(list.has_value());
    CHECK(list->folder == "Sounds/");
    CHECK(list->names == std::vector<std::string>{"engine.pcm", "Idle.pcm"});
}

TEST_CASE("a sound list loads only as many names as its count") {
    const Result<SoundList> list = ParseSoundList(Text("\r\n1\r\nstart.tun\r\nroyal.tun\r\n"));

    REQUIRE(list.has_value());
    CHECK(list->folder.empty());
    CHECK(list->names == std::vector<std::string>{"start.tun"});
}

TEST_CASE("a sound list rejects a bad count or too few names") {
    CHECK_FALSE(ParseSoundList(Text("\r\ntwo\r\na.pcm\r\nb.pcm\r\n")).has_value());
    CHECK_FALSE(ParseSoundList(Text("\r\n3\r\na.pcm\r\nb.pcm\r\n")).has_value());
    CHECK_FALSE(ParseSoundList(Text("\r\n2\r\na.pcm\r\n\r\nb.pcm\r\n")).has_value());
    CHECK_FALSE(ParseSoundList(Text("")).has_value());
}

TEST_CASE("every retail sound bank and music list parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    std::size_t soundCount = 0;
    const test::Survey files = test::SurveyLayer(
        *archive,
        [](std::string_view name) { return test::Extension(name) == "SBK" || name == "LEGOMSC"; },
        [&](std::span<const std::byte> bytes) {
            return ParseSoundList(bytes).transform(
                [&](const SoundList& list) { soundCount += list.names.size(); });
        });
    MESSAGE("SBK and LEGOMSC: ", files.fileCount, " files, ", soundCount, " names");
    test::CheckSurvey(files, 78);
    CHECK(soundCount == 1128);
}

}
}
