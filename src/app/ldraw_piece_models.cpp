#include "app/ldraw_piece_models.hpp"

#include <span>
#include <utility>

#include "assets/geometry_writer.hpp"
#include "assets/ldraw_extra_pieces.hpp"
#include "assets/ldraw_pieces.hpp"
#include "assets/piece_models.hpp"
#include "core/log.hpp"

namespace rr::app {

void LdrawPieceModels::DeclareSettings(settings::Registry& registry) const {
    registry.Declare(kLdrawPiecesSetting);
}

void LdrawPieceModels::Attach(mods::HookRegistry& hooks, const settings::Registry& settings) {
    hooks.Subscribe<mods::AssetResolving>([this, &settings](mods::AssetResolving& event) {
        if (event.request.generated || event.vfs.Contains(event.request.path)) {
            return;
        }
        const std::optional<std::string> piece = assets::PieceOfModelPath(event.request.path);
        if (!piece || (!settings.Get(kLdrawPiecesSetting) && !assets::IsExtraPiece(*piece))) {
            return;
        }
        if (std::optional<std::vector<std::byte>> file = ModelFile(*piece)) {
            event.request.generated = std::move(*file);
        }
    });
}

std::optional<std::vector<std::byte>> LdrawPieceModels::ModelFile(std::string_view pieceName) {
    const std::span<const assets::LdrawPart> parts = assets::LdrawPartsOf(pieceName);
    if (parts.empty()) {
        return std::nullopt;
    }
    const std::scoped_lock lock(mutex_);
    const auto [found, isNew] = files_.try_emplace(std::string(parts.front().piece));
    if (!isNew) {
        return found->second;
    }
    Result<std::vector<std::byte>> file =
        assets::LoadLdrawPiece(library_, parts).and_then([](const assets::GeometryAsset& model) {
            return assets::WriteGeometry(model, assets::GdbVertices::Lit);
        });
    if (!file) {
        log::Warn("Keeping piece {} because its LDraw parts can't be used: {}", parts.front().piece,
                  file.error().message);
        files_.erase(found);
        return std::nullopt;
    }
    found->second = std::move(*file);
    return found->second;
}

}
