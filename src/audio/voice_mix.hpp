#pragma once

namespace rr::audio {

/// Game units, not gain: `volume` 0 to 1, `pan` -1 left to 1 right. `pitch` scales the sample rate.
struct VoiceMix {
    float volume = 1.0F;
    float pitch = 1.0F;
    float pan = 0.0F;

    friend bool operator==(const VoiceMix&, const VoiceMix&) = default;
};

}
