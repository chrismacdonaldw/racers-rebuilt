#include "audio/looping_stream.hpp"

#include <doctest/doctest.h>

#include <array>
#include <memory>
#include <vector>

#include "audio/ramp_stream.hpp"

namespace rr::audio {
namespace {

std::vector<std::int16_t> ReadFrames(LoopingStream& stream, std::size_t frameCount) {
    std::vector<std::int16_t> samples(frameCount);
    CHECK(stream.Read(samples) == frameCount);
    return samples;
}

TEST_CASE("the loop region repeats after the loop end") {
    LoopingStream stream(std::make_unique<test::RampStream>(10), {.startFrame = 2, .endFrame = 6});

    CHECK(ReadFrames(stream, 12) == std::vector<std::int16_t>{0, 1, 2, 3, 4, 5, 2, 3, 4, 5, 2, 3});
}

TEST_CASE("without a loop end the whole stream repeats") {
    LoopingStream stream(std::make_unique<test::RampStream>(4), {});

    CHECK(ReadFrames(stream, 9) == std::vector<std::int16_t>{0, 1, 2, 3, 0, 1, 2, 3, 0});
}

TEST_CASE("an empty stream reads nothing instead of looping forever") {
    LoopingStream stream(std::make_unique<test::RampStream>(0), {});
    std::array<std::int16_t, 8> samples{};

    CHECK(stream.Read(samples) == 0);
}

}
}
