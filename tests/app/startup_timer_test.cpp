#include "app/startup_timer.hpp"

#include <doctest/doctest.h>

#include <chrono>

namespace rr::app {
namespace {

TEST_CASE("the startup summary lists each phase and the total") {
    using std::chrono::milliseconds;
    StartupTimer timer(milliseconds{100});

    timer.EndPhase("settings", milliseconds{102});
    timer.EndPhase("window", milliseconds{140});

    CHECK(timer.Summary() == "startup took 40.0 ms: settings 2.0 ms, window 38.0 ms");
}

}
}
