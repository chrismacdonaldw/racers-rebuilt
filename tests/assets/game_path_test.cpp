#include "assets/game_path.hpp"

#include <doctest/doctest.h>

namespace rr::assets {
namespace {

TEST_CASE("game paths are uppercased and use forward slashes") {
    CHECK(NormalizeGamePath("gamedata\\RaceC0R0/race.cpb") == "GAMEDATA/RACEC0R0/RACE.CPB");
}

TEST_CASE("leading and trailing separators are dropped") {
    CHECK(NormalizeGamePath("/GAMEDATA/") == "GAMEDATA");
    CHECK(NormalizeGamePath("") == "");
    CHECK(NormalizeGamePath("/") == "");
}

TEST_CASE("game paths reject empty, relative and non-ASCII components") {
    CHECK_FALSE(NormalizeGamePath("GAMEDATA//COMMON").has_value());
    CHECK_FALSE(NormalizeGamePath("GAMEDATA/../SAVE").has_value());
    CHECK_FALSE(NormalizeGamePath("./GAMEDATA").has_value());
    CHECK_FALSE(NormalizeGamePath("caf\xC3\xA9.txt").has_value());
    CHECK_FALSE(NormalizeGamePath(std::string_view("A\0B", 3)).has_value());
}

TEST_CASE("a game name can't contain a separator") {
    CHECK(NormalizeGameName("rrjmw.gdb") == "RRJMW.GDB");
    CHECK_FALSE(NormalizeGameName("A/B").has_value());
    CHECK_FALSE(NormalizeGameName("A\\B").has_value());
}

TEST_CASE("a game name can't contain characters Windows forbids in file names") {
    for (const std::string_view name : {"C:", "A*", "A?", "A\"B", "A<B", "A>B", "A|B"}) {
        CAPTURE(name);
        CHECK_FALSE(NormalizeGameName(name).has_value());
    }
}

TEST_CASE("joining onto the root folder gives the bare name") {
    CHECK(JoinGamePath("", "GAMEDATA") == "GAMEDATA");
    CHECK(JoinGamePath("GAMEDATA", "COMMON") == "GAMEDATA/COMMON");
}

}
}
