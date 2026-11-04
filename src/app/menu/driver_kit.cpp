#include "app/menu/driver_kit.hpp"

#include <algorithm>
#include <format>
#include <span>
#include <string_view>

#include "app/racer/racer_bounds.hpp"
#include "app/racer/racer_models.hpp"
#include "assets/driver_model.hpp"
#include "assets/game_path.hpp"
#include "assets/geometry_database.hpp"
#include "assets/model_files.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kShapeFolder = "MENUDATA/PARTDB/MENUPART";
constexpr std::string_view kHeads = "CBBODIES.GCB";
constexpr std::size_t kBareHeadHat = 0;
constexpr std::string_view kFaceShape = "HEADS";
constexpr std::array<std::string_view, 2> kTorsoShapes{"CHESTS", "CHEST_HK"};
constexpr std::array<std::string_view, 2> kLegsShapes{"LEGSS", "LEG-PEGS"};
constexpr std::array<std::string_view, 3> kDressSlots{"face", "torso", "legs"};

template <typename T>
Result<T> ReadParsed(const assets::Vfs& vfs, const std::string& path,
                     Result<T> (*parse)(std::span<const std::byte>)) {
    return vfs.Read(path).and_then(
        [parse](const assets::FileData& data) { return parse(data.Bytes()); });
}

std::string ShapePath(std::string_view name, std::string_view extension) {
    return std::format("{}/{}.{}", kShapeFolder, assets::ToUpperAscii(name), extension);
}

render::Material MaterialNamed(const assets::NameMap<render::Material>& materials,
                               std::string_view name) {
    const auto found = materials.find(assets::ToUpperAscii(name));
    return found == materials.end() ? render::Material{} : found->second;
}

std::optional<std::uint32_t> SlotNamed(const std::vector<std::string>& names,
                                       std::string_view name) {
    const auto found = std::ranges::find(names, name);
    if (found == names.end()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(found - names.begin());
}

struct Shape {
    JointedModel model;
    BoundingSphere bounds;
    std::optional<std::uint32_t> dress;
};

class PreviewBuilder {
public:
    PreviewBuilder(render::Renderer& renderer, const assets::Vfs& vfs,
                   const assets::DriverPartCatalog& catalog, const assets::PartLibrary& heads,
                   const assets::NameMap<render::Material>& materials)
        : renderer_(&renderer),
          vfs_(&vfs),
          catalog_(&catalog),
          heads_(&heads),
          materials_(&materials) {}

    Result<std::array<std::vector<PartPreview>, kDriverSlots>> Build() && {
        Result<void> built = Hats()
                                 .and_then([&] { return Faces(); })
                                 .and_then([&] { return Shelf(DriverSlot::Torso, kTorsoShapes); })
                                 .and_then([&] { return Shelf(DriverSlot::Legs, kLegsShapes); });
        if (!built) {
            return std::unexpected(std::move(built.error()));
        }
        return std::move(previews_);
    }

private:
    [[nodiscard]] Result<Shape> Upload(const assets::GeometryAsset& geometry,
                                       std::string_view dress) const {
        return UploadJointedModel(*renderer_, geometry, *materials_)
            .transform([&](JointedModel model) {
                return Shape{.model = std::move(model),
                             .bounds = ModelBounds(geometry.vertices),
                             .dress = SlotNamed(geometry.materials, dress)};
            });
    }

    [[nodiscard]] Result<Shape> LoadShape(std::string_view name, std::string_view dress) const {
        return ReadParsed(*vfs_, ShapePath(name, "GDB"), assets::ParseGeometry)
            .and_then(
                [&](const assets::GeometryAsset& geometry) { return Upload(geometry, dress); });
    }

    Result<void> Hats() {
        std::vector<PartPreview>& hats = PreviewsOf(DriverSlot::Hat);
        for (const assets::DriverPart& hat : catalog_->hats) {
            if (hats.size() == kBareHeadHat) {
                hats.emplace_back();
                continue;
            }
            const auto part =
                std::ranges::find(heads_->parts, hat.name, &assets::LibraryPart::name);
            const Result<assets::GeometryAsset> geometry =
                part == heads_->parts.end()
                    ? Result<assets::GeometryAsset>(assets::GeometryAsset{})
                    : assets::BuildPartGeometry(*heads_, hat.name, part->scale);
            if (!geometry || geometry->vertices.empty()) {
                log::Warn("{} has no hat {}, the driver editor shows none", kHeads, hat.name);
                hats.emplace_back();
                continue;
            }
            Result<Shape> shape = Upload(*geometry, {});
            if (!shape) {
                return std::unexpected(std::move(shape.error()));
            }
            hats.push_back({.draws = std::move(shape->model.draws), .bounds = shape->bounds});
        }
        return {};
    }

    Result<void> Faces() {
        return LoadShape(kFaceShape, kDressSlots[0]).transform([&](const Shape& shape) {
            for (const assets::DriverPart& face : catalog_->faces.parts) {
                PreviewsOf(DriverSlot::Face)
                    .push_back(Dressed(shape, assets::FaceMaterial(face.name, 0)));
            }
        });
    }

    Result<void> Shelf(DriverSlot slot, std::span<const std::string_view, 2> names) {
        const std::string_view dress = kDressSlots.at(static_cast<std::size_t>(slot) - 1);
        std::array<Shape, 2> shapes;
        for (std::size_t variant = 0; variant < shapes.size(); ++variant) {
            Result<Shape> shape = LoadShape(names[variant], dress);
            if (!shape) {
                return std::unexpected(std::move(shape.error()));
            }
            shapes.at(variant) = std::move(*shape);
        }
        const assets::DriverPartShelf& shelf =
            slot == DriverSlot::Torso ? catalog_->torsos : catalog_->legs;
        for (const assets::DriverPart& part : shelf.parts) {
            const auto variant = std::clamp<std::size_t>(static_cast<std::size_t>(part.variant), 0,
                                                         shapes.size() - 1);
            PreviewsOf(slot).push_back(Dressed(shapes.at(variant), part.name));
        }
        return {};
    }

    [[nodiscard]] PartPreview Dressed(const Shape& shape, std::string_view material) const {
        PartPreview preview{.draws = shape.model.draws, .bounds = shape.bounds};
        for (std::size_t draw = 0; draw < preview.draws.size(); ++draw) {
            if (shape.model.slots.at(draw) == shape.dress) {
                preview.draws[draw].material = MaterialNamed(*materials_, material);
            }
        }
        return preview;
    }

    std::vector<PartPreview>& PreviewsOf(DriverSlot slot) {
        return previews_.at(static_cast<std::size_t>(slot));
    }

    render::Renderer* renderer_;
    const assets::Vfs* vfs_;
    const assets::DriverPartCatalog* catalog_;
    const assets::PartLibrary* heads_;
    const assets::NameMap<render::Material>* materials_;
    std::array<std::vector<PartPreview>, kDriverSlots> previews_;
};

Result<assets::NameMap<render::Material>> UploadLooks(render::Renderer& renderer,
                                                      const assets::Vfs& vfs,
                                                      assets::TrackLoader& loader,
                                                      const assets::DriverPartCatalog& catalog) {
    return LoadDriverLooks(vfs, loader, catalog).and_then([&](const assets::TrackAssets& looks) {
        std::vector<std::string> names;
        for (const auto& [name, material] : looks.materials) {
            names.push_back(name);
        }
        return UploadMaterials(renderer, looks, names);
    });
}

}

Result<std::unique_ptr<DriverKit>> DriverKit::Load(render::Renderer& renderer,
                                                   const assets::Vfs& vfs,
                                                   assets::TrackLoader& loader) {
    Parts parts;
    Result<void> loaded =
        LoadDriverParts(vfs)
            .and_then([&](assets::DriverPartCatalog catalog) {
                parts.catalog = std::move(catalog);
                return LoadHeadParts(vfs, std::format("{}/{}", kShapeFolder, kHeads));
            })
            .and_then([&](assets::PartLibrary heads) {
                parts.heads = std::move(heads);
                return UploadLooks(renderer, vfs, loader, parts.catalog);
            })
            .and_then([&](assets::NameMap<render::Material> materials) {
                parts.materials = std::move(materials);
                return PreviewBuilder(renderer, vfs, parts.catalog, parts.heads, parts.materials)
                    .Build();
            })
            .transform([&](std::array<std::vector<PartPreview>, kDriverSlots> previews) {
                parts.previews = std::move(previews);
            });
    if (!loaded) {
        return Fail(loaded.error().code,
                    std::format("the driver editor's parts: {}", loaded.error().message));
    }
    return std::unique_ptr<DriverKit>(new DriverKit(renderer, vfs, std::move(parts)));
}

DriverKit::DriverKit(render::Renderer& renderer, const assets::Vfs& vfs, Parts parts)
    : renderer_(&renderer),
      vfs_(&vfs),
      catalog_(std::move(parts.catalog)),
      heads_(std::move(parts.heads)),
      materials_(std::move(parts.materials)),
      previews_(std::move(parts.previews)) {}

std::span<const PartPreview> DriverKit::Previews(DriverSlot slot) const {
    return previews_.at(static_cast<std::size_t>(slot));
}

Result<KitMinifig> DriverKit::Minifig(const assets::DriverLook& look) {
    return Build(look).transform([&](const Built* built) {
        KitMinifig minifig{
            .model = built->model, .skeleton = &built->body->skeleton, .scale = built->scale};
        for (std::size_t draw = 0; draw < minifig.model.draws.size(); ++draw) {
            for (std::size_t dress = 0; dress < kDresses; ++dress) {
                if (built->slots.at(dress) == minifig.model.slots.at(draw)) {
                    minifig.model.draws[draw].material = DressOf(look, static_cast<Dress>(dress));
                }
            }
        }
        return minifig;
    });
}

Result<const DriverKit::Body*> DriverKit::BodyOf(const std::string& name) {
    if (const auto found = bodies_.find(name); found != bodies_.end()) {
        return found->second.get();
    }
    auto body = std::make_unique<Body>();
    Result<void> loaded = ReadParsed(*vfs_, ShapePath(name, "GDB"), assets::ParseGeometry)
                              .and_then([&](assets::GeometryAsset geometry) {
                                  body->geometry = std::move(geometry);
                                  return LoadMenuMinifigSkeleton(*vfs_, name);
                              })
                              .transform([&](assets::SkeletonAsset skeleton) {
                                  body->skeleton = std::move(skeleton);
                              });
    if (!loaded) {
        return std::unexpected(std::move(loaded.error()));
    }
    return bodies_.emplace(name, std::move(body)).first->second.get();
}

Result<const DriverKit::Built*> DriverKit::Build(const assets::DriverLook& worn) {
    const assets::DriverLook look = assets::FitLook(catalog_, worn);
    Result<std::string> name = assets::DriverBodyName(catalog_, look, assets::DriverDetail::Menu);
    if (!name) {
        return std::unexpected(std::move(name.error()));
    }
    std::pair key{*name, look.hat};
    if (const auto found = built_.find(key); found != built_.end()) {
        return &found->second;
    }
    assets::DriverLook plain = look;
    plain.expression = 0;
    Result<const Body*> body = BodyOf(*name);
    Result<assets::GeometryAsset> geometry = body.and_then([&](const Body* loaded) {
        return assets::BuildDriverGeometry(catalog_, heads_, loaded->geometry, plain,
                                           assets::DriverDetail::Menu);
    });
    Result<JointedModel> model = geometry.and_then([&](const assets::GeometryAsset& built) {
        return UploadJointedModel(*renderer_, built, materials_);
    });
    if (!model) {
        return std::unexpected(std::move(model.error()));
    }
    Built built{.model = std::move(*model), .scale = geometry->scale, .body = *body};
    built.slots = {SlotNamed(geometry->materials,
                             assets::FaceMaterial(catalog_.faces.parts.at(look.face).name, 0)),
                   SlotNamed(geometry->materials, catalog_.torsos.parts.at(look.torso).name),
                   SlotNamed(geometry->materials, catalog_.legs.parts.at(look.legs).name)};
    return &built_.emplace(std::move(key), std::move(built)).first->second;
}

render::Material DriverKit::DressOf(const assets::DriverLook& worn, Dress dress) const {
    const assets::DriverLook look = assets::FitLook(catalog_, worn);
    switch (dress) {
        case Dress::Face:
            return MaterialNamed(materials_,
                                 assets::FaceMaterial(catalog_.faces.parts.at(look.face).name, 0));
        case Dress::Torso:
            return MaterialNamed(materials_, catalog_.torsos.parts.at(look.torso).name);
        case Dress::Legs:
            return MaterialNamed(materials_, catalog_.legs.parts.at(look.legs).name);
    }
    return {};
}

}
