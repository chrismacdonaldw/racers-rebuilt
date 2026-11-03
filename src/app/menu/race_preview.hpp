#pragma once

#include <string>

namespace rr::app {

/// A race's frame of the track preview cutscene, and the `DRIVERS.DDB` driver it shows.
struct RacePreview {
    std::string frame;
    std::string driver;

    friend bool operator==(const RacePreview&, const RacePreview&) = default;
};

}
