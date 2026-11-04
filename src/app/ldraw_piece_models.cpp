#include "app/ldraw_piece_models.hpp"

#include <utility>

#include "assets/geometry_writer.hpp"
#include "assets/ldraw_pieces.hpp"
#include "assets/piece_models.hpp"
#include "core/log.hpp"

namespace rr::app {

void LdrawPieceModels::DeclareSettings(settings::Registry& registry) const {
    registry.Declare(kLdrawPiecesSetting);
}

void LdrawPieceModels::Attach(mods::HookRegistry& hooks, const settings::Registry& settings) {
    hooks.Subscribe<mods::AssetResolving>([this, &settings](mods::AssetResolving& event) {
        if (event.request.generated || !settings.Get(kLdrawPiecesSetting) ||
            event.vfs.Contains(event.request.path)) {
            return;
        }
        const std::optional<std::string> piece = assets::PieceOfModelPath(event.request.path);
        if (!piece) {
            return;
        }
        if (std::optional<std::vector<std::byte>> file = ModelFile(*piece)) {
            event.request.generated = std::move(*file);
        }
    });
}

std::optional<std::vector<std::byte>> LdrawPieceModels::ModelFile(std::string_view pieceName) {
    const std::optional<assets::LdrawPiece> piece = assets::LdrawPieceFor(pieceName);
    if (!piece) {
        return std::nullopt;
    }
    const std::scoped_lock lock(mutex_);
    const auto [found, isNew] = files_.try_emplace(std::string(piece->piece));
    if (!isNew) {
        return found->second;
    }
    Result<std::vector<std::byte>> file =
        assets::LoadLdrawPiece(library_, *piece).and_then([](const assets::GeometryAsset& model) {
            return assets::WriteGeometry(model, assets::GdbVertices::Lit);
        });
    if (!file) {
        log::Warn("Keeping piece {} because LDraw part {} can't be used: {}", piece->piece,
                  piece->file, file.error().message);
        return std::nullopt;
    }
    found->second = std::move(*file);
    return found->second;
}

}
