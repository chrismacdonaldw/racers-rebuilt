#pragma once

namespace rr::audio {

/// Spreads the game's 0 to 1 volume evenly over 30 dB, as gain. Below 0.005 is silent.
[[nodiscard]] float VolumeGain(float volume);

/// Turns the game's pan, which cuts the far channel by up to 25 dB, into a miniaudio balance.
[[nodiscard]] float PanBalance(float pan);

}
