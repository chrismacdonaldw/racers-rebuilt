#include "sim/race/cheats.hpp"

#include <doctest/doctest.h>

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace rr::sim {
namespace {

Cheats Entered(std::initializer_list<std::string_view> plates) {
    Cheats cheats;
    for (const std::string_view plate : plates) {
        cheats.EnterPlate(plate);
    }
    return cheats;
}

std::string Plates(std::initializer_list<std::string_view> entered) {
    return CheatPlates(Entered(entered));
}

TEST_CASE("each plate toggles its own cheat bit") {
    CHECK(Entered({"NSLWJ"}).Bits() == 0x001);
    CHECK(Entered({"FLYSKYHGH"}).Bits() == 0x002);
    CHECK(Entered({"LNFRRRM"}).Bits() == 0x020);
    CHECK(Entered({"MXPMX"}).Bits() == 0x080);
    CHECK(Entered({"FSTFRWRD"}).Bits() == 0x100);
    CHECK(Entered({"NDRVR"}).Bits() == 0x800);
    CHECK(Entered({"MXPMX", "FSTFRWRD", "MXPMX"}).Bits() == 0x100);
}

TEST_CASE("a plate that isn't a cheat changes nothing") {
    Cheats cheats = Entered({"MXPMX"});

    CHECK_FALSE(cheats.EnterPlate("JOAN"));
    CHECK_FALSE(cheats.EnterPlate("mxpmx"));
    CHECK(cheats == Entered({"MXPMX"}));
}

TEST_CASE("NMRCHTS clears every cheat") {
    Cheats cheats = Entered({"MXPMX", "NWHLS", "FSTFRWRD"});

    CHECK(cheats.EnterPlate("NMRCHTS"));
    CHECK(cheats.IsEmpty());
}

TEST_CASE("the brick colour cheats and the red only cheat exclude each other") {
    CHECK(Plates({"PGLLRD", "PGLLYLL"}) == "PGLLYLL");
    CHECK(Plates({"PGLLYLL", "PGLLGRN"}) == "PGLLGRN");
    CHECK(Plates({"PGLLGRN", "PGLLRD"}) == "PGLLRD");
    CHECK(Plates({"RPCRNLY", "PGLLRD"}) == "PGLLRD");
    CHECK(Plates({"PGLLGRN", "RPCRNLY"}) == "RPCRNLY");
}

TEST_CASE("red only and max power exclude each other") {
    CHECK(Plates({"MXPMX", "RPCRNLY"}) == "RPCRNLY");
    CHECK(Plates({"RPCRNLY", "MXPMX"}) == "MXPMX");
}

TEST_CASE("turning a cheat off clears nothing else") {
    CHECK(Plates({"MXPMX", "PGLLRD", "PGLLRD"}) == "MXPMX");
}

TEST_CASE("the third hidden part clears one of the other two, as the original does") {
    CHECK(Plates({"NWHLS", "NCHSSS"}) == "NWHLS,NCHSSS");
    CHECK(Plates({"NCHSSS", "NDRVR", "NWHLS"}) == "NWHLS,NCHSSS");
    CHECK(Plates({"NWHLS", "NDRVR", "NCHSSS"}) == "NCHSSS,NDRVR");
    CHECK(Plates({"NWHLS", "NCHSSS", "NDRVR"}) == "NWHLS,NDRVR");
}

TEST_CASE("cheat plates read back to the same cheats") {
    const Cheats cheats = Entered({"FSTFRWRD", "NSLWJ", "PGLLGRN"});

    CHECK(CheatPlates(cheats) == "NSLWJ,PGLLGRN,FSTFRWRD");
    CHECK(ParseCheatPlates(CheatPlates(cheats)) == cheats);
    CHECK(PlateOf(Cheat::HoverTurbo) == "FLYSKYHGH");
}

TEST_CASE("cheat plates with an unknown or repeated plate don't read") {
    CHECK_FALSE(ParseCheatPlates("MXPMX,NOPE").has_value());
    CHECK_FALSE(ParseCheatPlates("MXPMX,MXPMX").has_value());
    CHECK_FALSE(ParseCheatPlates("NMRCHTS").has_value());
    CHECK_FALSE(ParseCheatPlates("MXPMX,").has_value());
}

}
}
