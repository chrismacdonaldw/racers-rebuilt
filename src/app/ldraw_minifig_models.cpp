#include "app/ldraw_minifig_models.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <string>
#include <utility>

#include "assets/figure_rig.hpp"
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
constexpr std::string_view kRigExtension = ".RIG";

using File = std::optional<std::vector<std::byte>>;

struct FittedFiles {
    std::vector<std::byte> body;
    std::vector<std::byte> rig;
};

std::vector<std::byte> BytesOf(std::string_view text) {
    std::vector<std::byte> bytes(text.size());
    std::ranges::transform(text, bytes.begin(), [](char c) { return static_cast<std::byte>(c); });
    return bytes;
}

std::string WithExtension(std::string_view path, std::string_view extension) {
    return std::format("{}{}", path.substr(0, path.rfind('.')), extension);
}

template <typename T>
void WarnUnfitted(std::string_view path, const Result<T>& fitted) {
    if (!fitted) {
        log::Warn("Keeping {} because its LDraw minifig can't be fitted: {}", path,
                  fitted.error().message);
    }
}

std::optional<std::string_view> MinifigFileName(std::string_view path) {
    for (const std::string_view folder : kMinifigFolders) {
        if (path.starts_with(folder)) {
            return path.substr(folder.size());
        }
    }
    return std::nullopt;
}

Result<FittedFiles> FitBody(assets::LdrawLibrary& library, const assets::Vfs& vfs,
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
    const std::string skeletonPath = WithExtension(path, kSkeletonExtension);
    const Result<assets::SkeletonAsset> skeleton =
        vfs.ReadOriginal(skeletonPath).and_then([](const assets::FileData& data) {
            return assets::ParseSkeleton(data.Bytes());
        });
    return skeleton
        .and_then([&](const assets::SkeletonAsset&) { return assets::ParseGeometry(file.Bytes()); })
        .and_then([&](const assets::GeometryAsset& body) {
            return assets::FitMinifigBody(body, *skeleton, pieces);
        })
        .and_then([](const assets::FittedMinifig& fitted) {
            return assets::WriteGeometry(fitted.geometry, assets::GdbVertices::Lit)
                .transform([&](std::vector<std::byte> body) {
                    return FittedFiles{.body = std::move(body),
                                       .rig = BytesOf(assets::WriteFigureRig(fitted.rig))};
                });
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

void LdrawMinifigModels::DeclareSettings(settings::Registry& registry) const {
    registry.Declare(kLdrawMinifigsSetting);
}

void LdrawMinifigModels::Attach(mods::HookRegistry& hooks, const settings::Registry& settings) {
    hooks.Subscribe<mods::AssetResolving>([this, &settings](mods::AssetResolving& event) {
        if (event.request.generated || !settings.Get(kLdrawMinifigsSetting)) {
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
    const bool isBody = name->ends_with(kBodyExtension) || name->ends_with(kRigExtension);
    const std::span<const assets::MinifigPiece> figure =
        isBody ? assets::MinifigFigureOf(name->substr(0, name->rfind('.')))
               : std::span<const assets::MinifigPiece>{};
    if (!isHeads && figure.empty()) {
        return std::nullopt;
    }
    const std::string source = isHeads ? std::string(path) : WithExtension(path, kBodyExtension);
    Result<std::vector<assets::FileData>> layers = vfs.ReadEveryLayer(source);
    if (!layers || layers->size() != 1) {
        return std::nullopt;
    }
    const std::scoped_lock lock(mutex_);
    if (const auto found = files_.find(std::string(path)); found != files_.end()) {
        return found->second;
    }
    if (isHeads) {
        Result<std::vector<std::byte>> heads = FitHeads(library_, layers->front());
        WarnUnfitted(source, heads);
        return files_[source] = heads ? File(std::move(*heads)) : File{};
    }
    Result<FittedFiles> fitted = FitBody(library_, vfs, layers->front(), source, figure);
    WarnUnfitted(source, fitted);
    const std::string rig = WithExtension(source, kRigExtension);
    files_[source] = fitted ? File(std::move(fitted->body)) : File{};
    files_[rig] = fitted ? File(std::move(fitted->rig)) : File{};
    return files_.at(std::string(path));
}

}
