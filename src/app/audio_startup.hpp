#pragma once

#include "assets/vfs.hpp"
#include "audio/mixer.hpp"
#include "core/result.hpp"
#include "settings/registry.hpp"

namespace rr::app {

/// Falls back to a silent mixer when there is no audio device.
[[nodiscard]] Result<audio::Mixer> StartAudio(const settings::Registry& registry,
                                              const assets::Vfs& vfs);

}
