#include "assets/piece_models.hpp"

#include <format>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/geometry_database.hpp"
#include "core/log.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kModelFolder = "MENUDATA/PIECEDB/MODELS/";
constexpr std::string_view kModelExtension = ".GDB";

}

std::string PieceModelPath(std::string_view pieceName) {
    return std::format("{}{}{}", kModelFolder, ToUpperAscii(pieceName), kModelExtension);
}

std::optional<std::string> PieceOfModelPath(std::string_view path) {
    if (!path.starts_with(kModelFolder) || !path.ends_with(kModelExtension)) {
        return std::nullopt;
    }
    path.remove_prefix(kModelFolder.size());
    path.remove_suffix(kModelExtension.size());
    if (path.empty() || path.contains('/')) {
        return std::nullopt;
    }
    return std::string(path);
}

PieceModels ReadPieceModels(const Vfs& vfs, const PieceLibrary& library) {
    PieceModels models;
    for (const PieceRecord& piece : library.pieces) {
        if (piece.IsChassis() || piece.type == kStudPieceType) {
            continue;
        }
        const std::string path = PieceModelPath(piece.name);
        Result<GeometryAsset> model = vfs.Read(path).and_then(
            [](const FileData& file) { return ParseGeometry(file.Bytes()); });
        if (model) {
            models.emplace(ToUpperAscii(piece.name), std::move(*model));
        } else if (model.error().code != ErrorCode::NotFound) {
            log::Warn("Keeping piece {} because {} can't be read: {}", piece.name, path,
                      model.error().message);
        }
    }
    return models;
}

}
