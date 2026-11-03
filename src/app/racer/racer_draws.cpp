#include "app/racer/racer_draws.hpp"

#include <format>
#include <string_view>
#include <utility>

#include "app/track_model.hpp"
#include "assets/game_path.hpp"

namespace rr::app {
namespace {

Result<ModelDraws> UploadPart(render::Renderer& renderer, const assets::TrackAssets& models,
                              std::string_view name) {
    const auto geometry = models.models.find(assets::ToUpperAscii(name));
    if (geometry == models.models.end()) {
        return Fail(ErrorCode::NotFound, std::format("no model named {}", name));
    }
    return UploadModel(renderer, models, name).transform([&](std::vector<render::MeshDraw> draws) {
        return FollowJoints(*geometry->second, std::move(draws));
    });
}

void Hide(ModelDraws& model) {
    for (render::MeshDraw& draw : model.draws) {
        draw.indexCount = 0;
    }
}

}

ModelDraws FollowJoints(const assets::GeometryAsset& geometry,
                        std::vector<render::MeshDraw> draws) {
    ModelDraws model{.draws = std::move(draws), .scale = geometry.scale};
    model.joints.reserve(geometry.ranges.size());
    for (const assets::GeometryRange& range : geometry.ranges) {
        model.joints.push_back(range.joint);
    }
    return model;
}

Result<RacerDraws> UploadRacer(render::Renderer& renderer, const RacerAssets& racer) {
    RacerDraws draws;
    return UploadPart(renderer, racer.models, racer.files.chassis.wheels.model.geometry)
        .and_then([&](ModelDraws wheels) {
            draws.wheels = std::move(wheels);
            return UploadPart(renderer, racer.models, racer.files.champion.body.geometry);
        })
        .and_then([&](ModelDraws body) {
            draws.body = std::move(body);
            return UploadPart(renderer, racer.models, racer.files.driver.model.geometry);
        })
        .transform([&](ModelDraws driver) {
            draws.driver = std::move(driver);
            return std::move(draws);
        });
}

RacerDraws ShowOnly(RacerDraws draws, RacerParts shown) {
    if (!shown.wheels) {
        Hide(draws.wheels);
    }
    if (!shown.body) {
        Hide(draws.body);
    }
    if (!shown.driver) {
        Hide(draws.driver);
    }
    return draws;
}

}
