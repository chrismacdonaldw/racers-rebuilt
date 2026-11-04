#include "app/ldraw_minifig_models.hpp"

#include <array>
#include <format>
#include <span>
#include <utility>

#include "app/ldraw_piece_models.hpp"
#include "assets/geometry_database.hpp"
#include "assets/geometry_writer.hpp"
#include "assets/ldraw_minifig.hpp"
#include "assets/minifig_fit.hpp"
#include "assets/part_library_writer.hpp"
#include "assets/skeleton_database.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::array<std::string_view, 2> kMinifigFolders{"MENUDATA/PARTDB/MENUPART/",
                                                          "MENUDATA/PARTDB/GAMEPART/"};
constexpr std::string_view kBodyExtension = ".GDB";
constexpr std::string_view kSkeletonExtension = ".SDB";
constexpr std::string_view kHeadsExtension = ".GCB";

std::optional<std::string_view> MinifigFileName(std::string_view path) {
    for (const std::string_view folder : kMinifigFolders) {
        if (path.starts_with(folder)) {
            return path.substr(folder.size());
        }
    }
    return std::nullopt;
}

Result<std::vector<std::byte>> FitBody(assets::LdrawLibrary& library, const assets::Vfs& vfs,
                                       const assets::FileData& file, std::string_view path,
                                       std::span<const assets::MinifigPiece> figure) {
    std::vector<assets::FigurePiece> pieces;
    for (const assets::MinifigPiece& piece : figure) {
        Result<assets::GeometryAsset> part = library.LoadPart(piece.file);
        if (!part) {
            return std::unexpected(std::move(part.error()));
        }
        pieces.push_back({.piece = &piece, .geometry = std::move(*part)});
    }
    const std::string skeletonPath = std::format(
        "{}{}", path.substr(0, path.size() - kBodyExtension.size()), kSkeletonExtension);
    const Result<assets::SkeletonAsset> skeleton =
        vfs.ReadOriginal(skeletonPath).and_then([](const assets::FileData& data) {
            return assets::ParseSkeleton(data.Bytes());
        });
    return skeleton
        .and_then([&](const assets::SkeletonAsset&) { return assets::ParseGeometry(file.Bytes()); })
        .and_then([&](const assets::GeometryAsset& body) {
            return assets::FitMinifigBody(body, *skeleton, pieces);
        })
        .and_then([](const assets::GeometryAsset& fitted) {
            return assets::WriteGeometry(fitted, assets::GdbVertices::Lit);
        });
}

Result<std::vector<std::byte>> FitHeads(assets::LdrawLibrary& library,
                                        const assets::FileData& file) {
    const Result<assets::GeometryAsset> head = library.LoadPart(assets::kMinifigHeadFile);
    return head
        .and_then(
            [&](const assets::GeometryAsset&) { return assets::ParsePartLibrary(file.Bytes()); })
        .and_then([&](assets::PartLibrary heads) {
            return assets::WritePartLibrary(assets::FitMinifigHeads(std::move(heads), *head));
        });
}

}

void LdrawMinifigModels::DeclareSettings([[maybe_unused]] settings::Registry& registry) const {}

void LdrawMinifigModels::Attach(mods::HookRegistry& hooks, const settings::Registry& settings) {
    hooks.Subscribe<mods::AssetResolving>([this, &settings](mods::AssetResolving& event) {
        if (event.request.generated || !settings.Get(kLdrawPiecesSetting)) {
            return;
        }
        if (std::optional<std::vector<std::byte>> file = ModelFile(event.vfs, event.request.path)) {
            event.request.generated = std::move(*file);
        }
    });
}

std::optional<std::vector<std::byte>> LdrawMinifigModels::ModelFile(const assets::Vfs& vfs,
                                                                    std::string_view path) {
    const std::optional<std::string_view> name = MinifigFileName(path);
    if (!name) {
        return std::nullopt;
    }
    const bool isHeads = name->ends_with(kHeadsExtension);
    const std::span<const assets::MinifigPiece> figure =
        name->ends_with(kBodyExtension)
            ? assets::MinifigFigureOf(name->substr(0, name->size() - kBodyExtension.size()))
            : std::span<const assets::MinifigPiece>{};
    if (!isHeads && figure.empty()) {
        return std::nullopt;
    }
    Result<std::vector<assets::FileData>> layers = vfs.ReadEveryLayer(path);
    if (!layers || layers->size() != 1) {
        return std::nullopt;
    }
    const std::scoped_lock lock(mutex_);
    const auto [found, isNew] = files_.try_emplace(std::string(path));
    if (!isNew) {
        return found->second;
    }
    Result<std::vector<std::byte>> file =
        isHeads ? FitHeads(library_, layers->front())
                : FitBody(library_, vfs, layers->front(), path, figure);
    if (!file) {
        log::Warn("Keeping {} because its LDraw minifig can't be fitted: {}", path,
                  file.error().message);
        return std::nullopt;
    }
    found->second = std::move(*file);
    return found->second;
}

}
