#include "app/test_scene.hpp"

#include <cmath>
#include <cstddef>
#include <utility>

#include "assets/geometry_asset.hpp"
#include "assets/image_asset.hpp"
#include "core/log.hpp"
#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr sim::EntityId kCubeId{1};
constexpr float kOrbitRadius = 2.5F;
constexpr float kOrbitRadiansPerTick = 0.012F;
constexpr float kSpinRadiansPerTick = 0.03F;
constexpr int kGridHalfExtent = 10;
constexpr std::uint32_t kCheckerSizePx = 64;
constexpr std::uint32_t kCheckerCellPx = 8;
constexpr Vector3 kUp{.y = 1.0F};
constexpr Transform kCameraTransform{.position = {.y = 3.0F, .z = 8.0F},
                                     .rotation = {.w = 0.9887711F, .x = -0.1494381F}};

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

constexpr Rgb kLight{.r = 230, .g = 200, .b = 40};
constexpr Rgb kDark{.r = 60, .g = 40, .b = 40};

struct CubeFace {
    Vector3 normal;
    Vector3 right;
    Vector3 up;
};

constexpr std::array<CubeFace, 6> kCubeFaces{{
    {.normal = {.x = 1.0F}, .right = {.z = -1.0F}, .up = {.y = 1.0F}},
    {.normal = {.x = -1.0F}, .right = {.z = 1.0F}, .up = {.y = 1.0F}},
    {.normal = {.y = 1.0F}, .right = {.x = 1.0F}, .up = {.z = -1.0F}},
    {.normal = {.y = -1.0F}, .right = {.x = 1.0F}, .up = {.z = 1.0F}},
    {.normal = {.z = 1.0F}, .right = {.x = 1.0F}, .up = {.y = 1.0F}},
    {.normal = {.z = -1.0F}, .right = {.x = -1.0F}, .up = {.y = 1.0F}},
}};

Vector3 Corner(const CubeFace& face, float right, float up) {
    return {.x = (face.normal.x + (face.right.x * right) + (face.up.x * up)) / 2.0F,
            .y = (face.normal.y + (face.right.y * right) + (face.up.y * up)) / 2.0F,
            .z = (face.normal.z + (face.right.z * right) + (face.up.z * up)) / 2.0F};
}

assets::GeometryAsset BuildCube() {
    constexpr std::array<std::array<float, 2>, 4> kCorners{{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}};
    assets::GeometryAsset cube;
    for (const CubeFace& face : kCubeFaces) {
        const auto first = static_cast<std::uint32_t>(cube.vertices.size());
        for (const auto [right, up] : kCorners) {
            cube.vertices.push_back({.position = Corner(face, right, up),
                                     .normal = face.normal,
                                     .u = (right + 1.0F) / 2.0F,
                                     .v = (1.0F - up) / 2.0F});
        }
        cube.indices.insert(cube.indices.end(),
                            {first, first + 1, first + 2, first, first + 2, first + 3});
    }
    return cube;
}

assets::ImageAsset BuildChecker() {
    assets::ImageAsset image{.widthPx = kCheckerSizePx, .heightPx = kCheckerSizePx};
    for (std::uint32_t y = 0; y < kCheckerSizePx; ++y) {
        for (std::uint32_t x = 0; x < kCheckerSizePx; ++x) {
            const std::uint32_t cellX = x / kCheckerCellPx;
            const std::uint32_t cellY = y / kCheckerCellPx;
            const bool isLight = (cellX + cellY) % 2 == 0;
            const bool isHole = cellX % 4 == 1 && cellY % 4 == 1;
            const std::array pixel{std::byte{isLight ? kLight.r : kDark.r},
                                   std::byte{isLight ? kLight.g : kDark.g},
                                   std::byte{isLight ? kLight.b : kDark.b},
                                   std::byte{isHole ? std::uint8_t{0} : std::uint8_t{255}}};
            image.rgba.insert(image.rgba.end(), pixel.begin(), pixel.end());
        }
    }
    return image;
}

std::vector<render::DebugLine> BuildGrid() {
    constexpr auto kExtent = static_cast<float>(kGridHalfExtent);
    constexpr render::Color kGridColor{.r = 0.3F, .g = 0.3F, .b = 0.35F};
    std::vector<render::DebugLine> lines;
    for (int i = -kGridHalfExtent; i <= kGridHalfExtent; ++i) {
        const auto offset = static_cast<float>(i);
        lines.push_back({.from = {.x = offset, .z = -kExtent},
                         .to = {.x = offset, .z = kExtent},
                         .color = kGridColor});
        lines.push_back({.from = {.x = -kExtent, .z = offset},
                         .to = {.x = kExtent, .z = offset},
                         .color = kGridColor});
    }
    lines.push_back({.to = {.y = 2.0F}, .color = {.r = 0.2F, .g = 0.9F, .b = 0.3F}});
    return lines;
}

}

Result<std::unique_ptr<TestScene>> TestScene::Create(render::Renderer& renderer) {
    Result<render::MeshId> cube = renderer.UploadMesh(BuildCube());
    Result<render::TextureId> checker =
        cube.and_then([&](render::MeshId) { return renderer.UploadTexture(BuildChecker()); });
    if (!checker) {
        return std::unexpected(std::move(checker.error()));
    }
    const render::MeshDraw draw{.mesh = *cube, .material = {.texture = *checker}};
    return std::unique_ptr<TestScene>(new TestScene(renderer, draw));
}

void TestScene::Tick([[maybe_unused]] const sim::InputFrame& input) {
    ++tick_;
}

void TestScene::Snapshot(sim::RenderSnapshot& out) const {
    const auto tick = static_cast<float>(tick_);
    const float orbit = tick * kOrbitRadiansPerTick;
    out.entities.clear();
    out.entities.push_back(
        {.id = kCubeId,
         .transform = {.position = {.x = kOrbitRadius * std::cos(orbit),
                                    .y = 0.5F,
                                    .z = kOrbitRadius * std::sin(orbit)},
                       .rotation = FromAxisAngle(kUp, tick * kSpinRadiansPerTick)}});
    out.camera = {.transform = kCameraTransform};
}

void TestScene::Render(const sim::RenderSnapshot& interpolated) {
    if (!interpolated.entities.empty()) {
        meshes_[0].transform = interpolated.entities.front().transform;
    }
    const render::RenderView view{
        .camera = {.transform = interpolated.camera.transform}, .meshes = meshes_, .lines = lines_};
    Result<void> drawn = renderer_->Draw(view);
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

TestScene::TestScene(render::Renderer& renderer, render::MeshDraw cube)
    : renderer_(&renderer), meshes_{cube}, lines_(BuildGrid()) {}

}
