#pragma once

namespace rr::audio {

/// `pitch` scales the sample rate. `pan` runs from -1, left, to 1, right.
struct VoiceMix {
    float volume = 1.0F;
    float pitch = 1.0F;
    float pan = 0.0F;

    friend bool operator==(const VoiceMix&, const VoiceMix&) = default;
};

}
