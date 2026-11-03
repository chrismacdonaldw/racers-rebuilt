#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rr::test {

inline constexpr std::uint32_t kGlbMagic = 0x46546C67;
inline constexpr std::uint32_t kGlbJsonChunk = 0x4E4F534A;
inline constexpr std::uint32_t kGlbBinChunk = 0x004E4942;
inline constexpr int kGltfTriangles = 4;
inline constexpr int kGltfPoints = 0;

struct GlbTriangle {
    std::string material = "STONE";
    std::array<float, 3> translation{};
    bool hasNormals = true;
    bool hasColors = false;
    int mode = kGltfTriangles;
    std::uint16_t lastIndex = 2;
    std::string normalType = "VEC3";
};

class GlbParts {
public:
    std::string Floats(std::span<const float> values, int count, std::string_view type) {
        return View(std::as_bytes(values), count, "5126", type);
    }

    std::string Indices(std::span<const std::uint16_t> values) {
        return View(std::as_bytes(values), static_cast<int>(values.size()), "5123", "SCALAR");
    }

    [[nodiscard]] std::vector<std::byte> Build(std::string_view mesh,
                                               std::string_view nodes) const {
        std::string json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)";
        json += R"("nodes":[)" + std::string(nodes) + R"(],"meshes":[)" + std::string(mesh) + "],";
        json += R"("buffers":[{"byteLength":)" + std::to_string(bin_.size()) + "}],";
        json += R"("bufferViews":[)" + views_ + R"(],"accessors":[)" + accessors_ + "]";
        json += materials_ + "}";
        json.append((4 - (json.size() % 4)) % 4, ' ');
        std::vector<std::byte> glb;
        Append(glb, kGlbMagic);
        Append(glb, 2);
        Append(glb, static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + bin_.size()));
        Append(glb, static_cast<std::uint32_t>(json.size()));
        Append(glb, kGlbJsonChunk);
        const std::span<const std::byte> text = std::as_bytes(std::span(json));
        glb.insert(glb.end(), text.begin(), text.end());
        Append(glb, static_cast<std::uint32_t>(bin_.size()));
        Append(glb, kGlbBinChunk);
        glb.insert(glb.end(), bin_.begin(), bin_.end());
        return glb;
    }

    void Material(std::string_view name) {
        materials_ = R"(,"materials":[{"name":")" + std::string(name) + R"("}])";
    }

private:
    static void Append(std::vector<std::byte>& out, std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            out.push_back(static_cast<std::byte>(value >> shift));
        }
    }

    std::string View(std::span<const std::byte> bytes, int count, std::string_view component,
                     std::string_view type) {
        const std::string index = std::to_string(viewCount_++);
        Separate(views_);
        views_ += R"({"buffer":0,"byteOffset":)" + std::to_string(bin_.size()) +
                  R"(,"byteLength":)" + std::to_string(bytes.size()) + "}";
        Separate(accessors_);
        accessors_ += R"({"bufferView":)" + index + R"(,"componentType":)" +
                      std::string(component) + R"(,"count":)" + std::to_string(count) +
                      R"(,"type":")" + std::string(type) + R"("})";
        bin_.insert(bin_.end(), bytes.begin(), bytes.end());
        bin_.resize((bin_.size() + 3) / 4 * 4);
        return index;
    }

    static void Separate(std::string& list) {
        if (!list.empty()) {
            list += ',';
        }
    }

    std::vector<std::byte> bin_;
    std::string views_;
    std::string accessors_;
    std::string materials_;
    int viewCount_ = 0;
};

/// A parent node moved by `translation` holds the triangle (0,0,0) (1,0,0) (0,1,0) facing +Z.
[[nodiscard]] inline std::vector<std::byte> BuildGlb(const GlbTriangle& triangle) {
    constexpr std::array kPositions{0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    constexpr std::array kNormals{0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F};
    constexpr std::array kUvs{0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F};
    constexpr std::array kColors{1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F,
                                 0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 1.0F};
    GlbParts parts;
    std::string attributes = R"("POSITION":)" + parts.Floats(kPositions, 3, "VEC3");
    if (triangle.hasNormals) {
        attributes += R"(,"NORMAL":)" + parts.Floats(kNormals, 3, triangle.normalType);
    }
    attributes += R"(,"TEXCOORD_0":)" + parts.Floats(kUvs, 3, "VEC2");
    if (triangle.hasColors) {
        attributes += R"(,"COLOR_0":)" + parts.Floats(kColors, 3, "VEC4");
    }
    std::string primitive = R"({"attributes":{)" + attributes + R"(},"indices":)" +
                            parts.Indices(std::array<std::uint16_t, 3>{0, 1, triangle.lastIndex}) +
                            R"(,"mode":)" + std::to_string(triangle.mode);
    if (!triangle.material.empty()) {
        parts.Material(triangle.material);
        primitive += R"(,"material":0)";
    }
    const auto& [x, y, z] = triangle.translation;
    const std::string nodes = R"({"children":[1],"translation":[)" + std::to_string(x) + "," +
                              std::to_string(y) + "," + std::to_string(z) + R"(]},{"mesh":0})";
    return parts.Build(R"({"primitives":[)" + primitive + "}]}", nodes);
}

}
