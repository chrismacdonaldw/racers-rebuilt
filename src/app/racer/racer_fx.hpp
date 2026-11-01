#pragma once

#include "assets/track_loader.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// Empty, with a warning, when `RACERFX` fails to load.
[[nodiscard]] assets::NameMap<render::Material> LoadRacerFxMaterials(render::Renderer& renderer,
                                                                     assets::TrackLoader& loader);

}
