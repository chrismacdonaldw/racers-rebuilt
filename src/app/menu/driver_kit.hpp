#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "app/menu/carousel_layout.hpp"
#include "app/menu/part_carousel.hpp"
#include "app/track_model.hpp"
#include "assets/driver_look.hpp"
#include "assets/driver_parts.hpp"
#include "assets/geometry_asset.hpp"
#include "assets/part_library.hpp"
#include "assets/skeleton_database.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/bounding_sphere.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

struct KitMinifig {
    JointedModel model;
    const assets::SkeletonAsset* skeleton = nullptr;
    float scale = 1.0F;
};

/// Uploads each mesh once, as only a scene change frees them. `renderer` and `vfs` must outlive it.
class DriverKit {
public:
    [[nodiscard]] static Result<std::unique_ptr<DriverKit>> Load(render::Renderer& renderer,
                                                                 const assets::Vfs& vfs,
                                                                 assets::TrackLoader& loader);

    [[nodiscard]] const assets::DriverPartCatalog& Catalog() const { return catalog_; }

    [[nodiscard]] std::span<const PartPreview> Previews(DriverSlot slot) const;

    /// With the default face, as the editor shows it.
    [[nodiscard]] Result<KitMinifig> Minifig(const assets::DriverLook& look);

private:
    enum class Dress : std::uint8_t { Face, Torso, Legs };
    static constexpr std::size_t kDresses = 3;

    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Body {
        assets::GeometryAsset geometry;
        assets::SkeletonAsset skeleton;
    };

    struct Built {
        JointedModel model;
        std::array<std::optional<std::uint32_t>, kDresses> slots;
        float scale = 1.0F;
        const Body* body = nullptr;
    };

    // NOLINTNEXTLINE(bugprone-exception-escape)
    struct Parts {
        assets::DriverPartCatalog catalog;
        assets::PartLibrary heads;
        assets::NameMap<render::Material> materials;
        std::array<std::vector<PartPreview>, kDriverSlots> previews;
    };

    DriverKit(render::Renderer& renderer, const assets::Vfs& vfs, Parts parts);

    [[nodiscard]] Result<const Body*> BodyOf(const std::string& name);
    [[nodiscard]] Result<const Built*> Build(const assets::DriverLook& worn);
    [[nodiscard]] render::Material DressOf(const assets::DriverLook& worn, Dress dress) const;

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    assets::DriverPartCatalog catalog_;
    assets::PartLibrary heads_;
    assets::NameMap<render::Material> materials_;
    std::array<std::vector<PartPreview>, kDriverSlots> previews_;
    std::map<std::string, std::unique_ptr<Body>, std::less<>> bodies_;
    std::map<std::pair<std::string, std::uint8_t>, Built> built_;
};

}
