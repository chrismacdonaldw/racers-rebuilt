#include "assets/piece_merge.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "assets/ldraw_extra_pieces.hpp"

namespace rr::assets {
namespace {

constexpr std::uint16_t kModeMask = 0x3000;
constexpr std::uint16_t kTexturedMode = 0x1000;
constexpr std::uint16_t kQuadMode = 0x2000;
constexpr std::uint16_t kNormalPerCorner = 0x4000;
constexpr std::uint16_t kSharedNormal = 0x8000;
constexpr std::uint16_t kMaterialMask = 0x07FF;
constexpr std::uint16_t kTextureBit = 0x0800;
constexpr std::size_t kFirstOwnMaterial = 3;
constexpr std::size_t kTriangleCorners = 3;
constexpr std::size_t kMaxIndex = 0xFFFF;

struct PoolBases {
    std::size_t positions = 0;
    std::size_t normals = 0;
    std::size_t texCoords = 0;
};

class CommandCopy {
public:
    CommandCopy(const PieceLibrary& from, const PoolBases& bases,
                std::span<const std::size_t> materials, std::vector<std::uint16_t>& out)
        : from_(&from), bases_(bases), materials_(materials), out_(&out) {}

    [[nodiscard]] bool Copy(const PieceRecord& piece) {
        cursor_ = piece.indexOffset;
        bool hasTriangle = false;
        for (std::uint32_t left = piece.commandCount; left > 0 && isValid_; --left) {
            const std::uint16_t command = Next();
            if ((command & kModeMask) == kQuadMode) {
                isValid_ = isValid_ && hasTriangle;
                out_->push_back(command);
                Corner(hasNormalIndex_);
                continue;
            }
            hasTexture_ = (command & kModeMask) == kTexturedMode;
            hasNormalIndex_ = (command & kNormalPerCorner) != 0;
            const bool hasSharedNormal = (command & kSharedNormal) != 0;
            out_->push_back(Remapped(command));
            for (std::size_t corner = 0; corner < kTriangleCorners; ++corner) {
                Corner(!hasSharedNormal && (hasNormalIndex_ || corner == 0));
            }
            hasTriangle = true;
        }
        return isValid_;
    }

private:
    std::uint16_t Next() {
        if (cursor_ >= from_->indices.size()) {
            isValid_ = false;
            return 0;
        }
        return from_->indices[cursor_++];
    }

    void Shifted(std::size_t base, std::size_t poolSize) {
        const std::uint16_t index = Next();
        const std::size_t shifted = base + index;
        isValid_ = isValid_ && index < poolSize && shifted <= kMaxIndex;
        out_->push_back(static_cast<std::uint16_t>(shifted));
    }

    void Corner(bool readsNormal) {
        Shifted(bases_.positions, from_->positions.size());
        if (readsNormal) {
            Shifted(bases_.normals, from_->normals.size());
        }
        if (hasTexture_) {
            Shifted(bases_.texCoords, from_->texCoords.size());
        }
    }

    std::uint16_t Remapped(std::uint16_t command) {
        const std::size_t material = command & kMaterialMask;
        if (material < kFirstOwnMaterial) {
            return command;
        }
        const std::size_t shown = material + ((command & kTextureBit) != 0 ? 1 : 0);
        if (shown >= materials_.size()) {
            isValid_ = false;
            return command;
        }
        const std::size_t mapped = materials_[material];
        isValid_ =
            isValid_ && mapped <= kMaterialMask && materials_[shown] == mapped + (shown - material);
        return static_cast<std::uint16_t>((command & ~kMaterialMask) | (mapped & kMaterialMask));
    }

    const PieceLibrary* from_;
    PoolBases bases_;
    std::span<const std::size_t> materials_;
    std::vector<std::uint16_t>* out_;
    std::size_t cursor_ = 0;
    bool isValid_ = true;
    bool hasTexture_ = false;
    bool hasNormalIndex_ = false;
};

std::vector<std::size_t> MapMaterials(std::vector<std::string>& merged,
                                      const std::vector<std::string>& own) {
    if (own.empty()) {
        std::vector<std::size_t> same(merged.size());
        for (std::size_t i = 0; i < same.size(); ++i) {
            same[i] = i;
        }
        return same;
    }
    std::vector<std::size_t> mapped;
    mapped.reserve(own.size());
    for (const std::string& name : own) {
        const std::string upper = ToUpperAscii(name);
        const auto found = std::ranges::find_if(
            merged, [&](const std::string& kept) { return ToUpperAscii(kept) == upper; });
        mapped.push_back(static_cast<std::size_t>(found - merged.begin()));
        if (found == merged.end()) {
            merged.push_back(name);
        }
    }
    return mapped;
}

PieceRecord* FindNamed(PieceLibrary& library, std::string_view name) {
    const std::string upper = ToUpperAscii(name);
    const auto found = std::ranges::find_if(library.pieces, [&](const PieceRecord& piece) {
        return ToUpperAscii(piece.name) == upper;
    });
    return found == library.pieces.end() ? nullptr : &*found;
}

std::optional<std::string> TakeProblem(const PieceLibrary& library, const PieceRecord& piece,
                                       const PieceRecord* same) {
    if (same != nullptr) {
        return same->type == piece.type
                   ? std::nullopt
                   : std::optional<std::string>("it changes the game piece's type");
    }
    if (piece.type < kFirstBrickType || piece.type >= kFirstExtraPieceType) {
        return std::format("a new piece needs a type from {:#x} to {:#x}", kFirstBrickType,
                           kFirstExtraPieceType - 1);
    }
    if (std::ranges::find(library.pieces, piece.type, &PieceRecord::type) != library.pieces.end()) {
        return std::format("another piece has type {:#x}", piece.type);
    }
    return std::nullopt;
}

void TakePiece(PieceLayer& merged, const PieceLibrary& from, const PoolBases& bases,
               std::span<const std::size_t> materials, const PieceRecord& piece,
               std::vector<std::string>& problems) {
    PieceLibrary& library = merged.library;
    PieceRecord* same = FindNamed(library, piece.name);
    if (const std::optional<std::string> problem = TakeProblem(library, piece, same)) {
        problems.push_back(std::format("mod piece {}: {}", piece.name, *problem));
        return;
    }
    const std::size_t offset = library.indices.size();
    CommandCopy copy(from, bases, materials, library.indices);
    if (!copy.Copy(piece)) {
        library.indices.resize(offset);
        problems.push_back(
            std::format("mod piece {}: its commands read past its library", piece.name));
        return;
    }
    PieceRecord taken = piece;
    taken.indexOffset = static_cast<std::uint32_t>(offset);
    if (taken.modelOffsetPosition) {
        taken.modelOffsetPosition =
            static_cast<std::uint16_t>(*taken.modelOffsetPosition + bases.positions);
    }
    if (same != nullptr) {
        *same = std::move(taken);
    } else {
        library.pieces.push_back(std::move(taken));
    }
}

template <typename Element>
void Append(std::vector<Element>& to, const std::vector<Element>& from) {
    to.insert(to.end(), from.begin(), from.end());
}

void TakeLayer(PieceLayer& merged, const PieceLayer& layer, std::vector<std::string>& problems) {
    PieceLibrary& library = merged.library;
    const PoolBases bases{.positions = library.positions.size(),
                          .normals = library.normals.size(),
                          .texCoords = library.texCoords.size()};
    const bool fits = bases.positions + layer.library.positions.size() <= kMaxIndex + 1 &&
                      bases.normals + layer.library.normals.size() <= kMaxIndex + 1 &&
                      bases.texCoords + layer.library.texCoords.size() <= kMaxIndex + 1;
    if (!fits) {
        problems.emplace_back("a mod's pieces: their vertices don't fit beside the game's");
        return;
    }
    const std::vector<std::size_t> materials = MapMaterials(merged.materials, layer.materials);
    Append(library.positions, layer.library.positions);
    Append(library.normals, layer.library.normals);
    Append(library.texCoords, layer.library.texCoords);
    for (const PieceRecord& piece : layer.library.pieces) {
        TakePiece(merged, layer.library, bases, materials, piece, problems);
    }
}

}

PieceLayer MergePieceLayers(std::vector<PieceLayer> layers, std::vector<std::string>& problems) {
    if (layers.empty()) {
        return {};
    }
    PieceLayer merged = std::move(layers.front());
    for (const PieceLayer& layer : std::span(layers).subspan(1)) {
        TakeLayer(merged, layer, problems);
    }
    std::ranges::stable_sort(merged.library.pieces, {}, [](const PieceRecord& piece) {
        return std::pair{piece.type, piece.commandCount};
    });
    return merged;
}

}
