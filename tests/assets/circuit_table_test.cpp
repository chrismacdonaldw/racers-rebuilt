#include "assets/circuit_table.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rr::assets {
namespace {

std::vector<CircuitPatch> Parse(std::string_view text) {
    Result<std::vector<CircuitPatch>> patches = ParseCircuitTable(text, "CIRCUITS.TOML");
    REQUIRE_MESSAGE(patches.has_value(), patches.error().message);
    return std::move(*patches);
}

std::string ErrorFor(std::string_view text) {
    const Result<std::vector<CircuitPatch>> patches = ParseCircuitTable(text, "CIRCUITS.TOML");
    REQUIRE_FALSE(patches.has_value());
    return patches.error().message;
}

CircuitPatch NewCircuit(std::string name, std::string after) {
    return {.name = std::move(name),
            .title = "NEW CUP",
            .grid = std::vector<std::string>{"VV", "MG"},
            .races = std::vector<CircuitRace>{{.track = "LAVALANE"}},
            .after = std::move(after)};
}

struct Story {
    std::vector<CircuitEntry> circuits{
        {.name = "c0", .next = "c1"}, {.name = "c1", .next = "c2"}, {.name = "c2"}};
    std::vector<RaceEntry> races{{.name = "RACEC0R0",
                                  .folder = "racec0r0",
                                  .circuit = "c0",
                                  .nameLine = 4,
                                  .previewScene = "scene",
                                  .previewDriver = "CR"},
                                 {.name = "RACEC0R0M",
                                  .folder = "racec0r0",
                                  .circuit = "c1",
                                  .isMirrored = true,
                                  .previewScene = "mirrored"}};

    std::vector<std::string> Apply(const std::vector<CircuitPatch>& patches) {
        return ApplyCircuitPatches(patches, circuits, races);
    }

    [[nodiscard]] std::vector<std::string> Order() const {
        std::vector<std::string> order;
        for (std::optional<std::string> at = "c0"; at && order.size() <= circuits.size();) {
            order.push_back(*at);
            const auto found = std::ranges::find(circuits, *at, &CircuitEntry::name);
            at = found == circuits.end() ? std::nullopt : found->next;
        }
        return order;
    }
};

TEST_CASE("a circuit table reads every key, with races as folders or tables") {
    const std::vector<CircuitPatch> patches = Parse(
        "[[circuit]]\nname = \"lava-cup\"\ntitle = \"LAVA CUP\"\nboss = \"MG\"\n"
        "grid = [\"VV\", \"MG\", \"LV\"]\n"
        "races = [\"lavalane\", { track = \"RACEC2R1\" }, { track = \"lavalane\", mirrored = true "
        "}]\n"
        "after = \"c2\"\npart_set = \"lava-parts\"\nintro = \"circuit3\"\n"
        "[[circuit]]\nname = \"c3\"\nboss = \"KK\"\n");

    CHECK(patches ==
          std::vector<CircuitPatch>{
              {.name = "lava-cup",
               .title = "LAVA CUP",
               .boss = "MG",
               .grid = std::vector<std::string>{"VV", "MG", "LV"},
               .races = std::vector<CircuitRace>{{.track = "LAVALANE"},
                                                 {.track = "RACEC2R1"},
                                                 {.track = "LAVALANE", .isMirrored = true}},
               .after = "c2",
               .partSet = "lava-parts",
               .intro = "circuit3"},
              {.name = "c3", .boss = "KK"}});
    CHECK(Parse("").empty());
}

TEST_CASE("a circuit table rejects bad names, grids and races") {
    const std::vector<std::pair<std::string_view, std::string_view>> cases{
        {"[[circuit]]\nname = \"Lava Cup\"\n",
         "CIRCUITS.TOML: circuit 1: name must be lowercase letters, digits, - or _"},
        {"[[circuit]]\nname = \"a\"\ngrid = []\n",
         "CIRCUITS.TOML: circuit 1: grid must list 1 to 6 driver codes"},
        {"[[circuit]]\nname = \"a\"\nraces = [\"a\", \"b\", \"c\", \"d\", \"e\"]\n",
         "CIRCUITS.TOML: circuit 1: races must list 1 to 4 races"},
        {"[[circuit]]\nname = \"a\"\nraces = [{ mirrored = true }]\n",
         "CIRCUITS.TOML: circuit 1: each race must be a track folder or a table with track and "
         "mirrored"},
        {"[[circuit]]\nname = \"a\"\ntitle = 3\n", "CIRCUITS.TOML: circuit 1: title must be text"},
        {"circuit = 3\n", "CIRCUITS.TOML: circuits must be [[circuit]] tables"}};

    for (const auto& [text, error] : cases) {
        CAPTURE(text);
        CHECK(ErrorFor(text) == error);
    }
    CHECK(ErrorFor("[[circuit]\n").starts_with("CIRCUITS.TOML line 1:"));
}

TEST_CASE("a new circuit joins the story after the circuit it names") {
    Story story;

    REQUIRE(story.Apply({NewCircuit("lava-cup", "c1")}).empty());

    CHECK(story.Order() == std::vector<std::string>{"c0", "c1", "lava-cup", "c2"});
    CHECK(story.circuits.back().isFromMod);
    CHECK(story.races.back().circuit == "lava-cup");
    CHECK(story.races.back().folder == "LAVALANE");
}

TEST_CASE("circuits placed after the same one keep their load order, and can chain") {
    Story story;

    REQUIRE(story
                .Apply({NewCircuit("first", "c1"), NewCircuit("second", "c1"),
                        NewCircuit("third", "first")})
                .empty());

    CHECK(story.Order() == std::vector<std::string>{"c0", "c1", "first", "third", "second", "c2"});
}

TEST_CASE("a circuit placed after one with its own followers goes after them") {
    Story story;

    REQUIRE(story
                .Apply({NewCircuit("first", "c1"), NewCircuit("inner", "first"),
                        NewCircuit("second", "c1")})
                .empty());

    CHECK(story.Order() == std::vector<std::string>{"c0", "c1", "first", "inner", "second", "c2"});
}

TEST_CASE("a new grid makes its slot 1 the boss unless the table names one") {
    Story story;

    REQUIRE(story
                .Apply({NewCircuit("lava-cup", "c1"),
                        {.name = "c0", .grid = std::vector<std::string>{"VV", "LV"}},
                        {.name = "c1", .boss = "KK", .grid = std::vector<std::string>{"VV", "LV"}}})
                .empty());

    CHECK(story.circuits.back().boss == "MG");
    CHECK(story.circuits[0].boss == "LV");
    CHECK(story.circuits[1].boss == "KK");
    CHECK(story.circuits[2].boss.empty());
}

TEST_CASE("a circuit table changes a circuit in place") {
    Story story;

    REQUIRE(
        story.Apply({{.name = "c0", .boss = "MG", .partSet = "lava-parts", .intro = ""}}).empty());

    const CircuitEntry& c0 = story.circuits.front();
    CHECK(c0.boss == "MG");
    CHECK(c0.namedPartSet == "lava-parts");
    CHECK(c0.intro == "");
    CHECK_FALSE(c0.isFromMod);
}

TEST_CASE("a circuit's new races replace its own and copy a retail race's preview") {
    Story story;

    REQUIRE(
        story
            .Apply({{.name = "c0",
                     .races = std::vector<CircuitRace>{{.track = "LAVALANE"},
                                                       {.track = "RACEC0R0", .isMirrored = true}}}})
            .empty());

    REQUIRE(story.races.size() == 4);
    CHECK_FALSE(story.races.front().circuit.has_value());
    const RaceEntry& mirrored = story.races.back();
    CHECK(mirrored.circuit == "c0");
    CHECK(mirrored.position == 1);
    CHECK(mirrored.previewScene == "mirrored");
}

TEST_CASE("a circuit table change that can't apply is skipped with its reason") {
    Story story;
    CircuitPatch untitled = NewCircuit("lava-cup", "c0");
    untitled.title.reset();

    const std::vector<std::string> problems =
        story.Apply({NewCircuit("c2", "c0"), untitled, NewCircuit("lost", "c9")});

    CHECK(problems == std::vector<std::string>{
                          "MENUDATA/CIRCUITS.TOML: circuit c2: after only places a new circuit",
                          "MENUDATA/CIRCUITS.TOML: circuit lava-cup: a new circuit needs after, "
                          "title, grid and races",
                          "MENUDATA/CIRCUITS.TOML: circuit lost: after names no circuit c9"});
    CHECK(story.circuits.size() == 3);
    CHECK(story.races.size() == 2);
}

}
}
