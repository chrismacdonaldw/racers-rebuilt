#include "assets/glb_writer.hpp"

#include <algorithm>
#include <format>
#include <limits>

namespace rr::assets {
namespace {

constexpr std::uint32_t kMagic = 0x46546C67;
constexpr std::uint32_t kVersion = 2;
constexpr std::uint32_t kJsonChunk = 0x4E4F534A;
constexpr std::uint32_t kBinChunk = 0x004E4942;
constexpr std::size_t kHeaderBytes = 12;
constexpr std::size_t kChunkHeaderBytes = 8;
constexpr std::size_t kAlignment = 4;
constexpr int kFloatComponent = 5126;
constexpr int kUnsignedShortComponent = 5123;
constexpr int kUnsignedIntComponent = 5125;
constexpr int kArrayBufferTarget = 34962;
constexpr int kElementBufferTarget = 34963;
constexpr char kFirstPrintable = 0x20;

void Append(std::vector<std::byte>& out, std::uint32_t value) {
    for (std::uint32_t shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

std::size_t ComponentsOf(std::string_view type) {
    if (type == "VEC2") {
        return 2;
    }
    if (type == "VEC3") {
        return 3;
    }
    if (type == "VEC4") {
        return 4;
    }
    if (type == "MAT4") {
        return 16;
    }
    return 1;
}

std::string Bounds(std::span<const float> values, std::size_t components) {
    std::vector<float> low(components, std::numeric_limits<float>::max());
    std::vector<float> high(components, std::numeric_limits<float>::lowest());
    for (std::size_t i = 0; i < values.size(); ++i) {
        low[i % components] = std::min(low[i % components], values[i]);
        high[i % components] = std::max(high[i % components], values[i]);
    }
    const auto list = [](const std::vector<float>& numbers) {
        std::string text;
        for (const float number : numbers) {
            text += std::format("{}{}", text.empty() ? "" : ",", number);
        }
        return text;
    };
    return std::format(R"(,"min":[{}],"max":[{}])", list(low), list(high));
}

}

std::string JsonString(std::string_view text) {
    std::string quoted = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            quoted += '\\';
            quoted += c;
        } else if (c >= 0 && c < kFirstPrintable) {
            quoted += std::format("\\u{:04x}", static_cast<int>(c));
        } else {
            quoted += c;
        }
    }
    return quoted + "\"";
}

std::size_t GlbWriter::AddFloats(std::span<const float> values, std::string_view type,
                                 bool hasBounds) {
    const std::size_t components = ComponentsOf(type);
    const std::string bounds = hasBounds ? Bounds(values, components) : std::string();
    const bool isVertexData = type != "SCALAR" && type != "MAT4";
    return Add(std::as_bytes(values), values.size() / components, kFloatComponent, type, bounds,
               isVertexData ? std::format(R"(,"target":{})", kArrayBufferTarget) : "");
}

std::size_t GlbWriter::AddJoints(std::span<const std::uint16_t> values) {
    return Add(std::as_bytes(values), values.size() / 4, kUnsignedShortComponent, "VEC4", "",
               std::format(R"(,"target":{})", kArrayBufferTarget));
}

std::size_t GlbWriter::AddIndices(std::span<const std::uint32_t> values) {
    return Add(std::as_bytes(values), values.size(), kUnsignedIntComponent, "SCALAR", "",
               std::format(R"(,"target":{})", kElementBufferTarget));
}

std::size_t GlbWriter::Add(std::span<const std::byte> bytes, std::size_t count, int component,
                           std::string_view type, std::string_view bounds,
                           std::string_view target) {
    const std::size_t index = count_++;
    views_ += std::format(R"({}{{"buffer":0,"byteOffset":{},"byteLength":{}{}}})",
                          views_.empty() ? "" : ",", bin_.size(), bytes.size(), target);
    accessors_ +=
        std::format(R"({}{{"bufferView":{},"componentType":{},"count":{},"type":"{}"{}}})",
                    accessors_.empty() ? "" : ",", index, component, count, type, bounds);
    bin_.insert(bin_.end(), bytes.begin(), bytes.end());
    bin_.resize((bin_.size() + kAlignment - 1) / kAlignment * kAlignment);
    return index;
}

std::vector<std::byte> GlbWriter::Build(std::string_view fields) const {
    std::string json = std::format(
        R"({{"asset":{{"version":"2.0","generator":"racers-rebuilt"}},{},"buffers":[{{"byteLength":{}}}],"bufferViews":[{}],"accessors":[{}]}})",
        fields, bin_.size(), views_, accessors_);
    json.append((kAlignment - (json.size() % kAlignment)) % kAlignment, ' ');
    std::vector<std::byte> glb;
    glb.reserve(kHeaderBytes + (2 * kChunkHeaderBytes) + json.size() + bin_.size());
    Append(glb, kMagic);
    Append(glb, kVersion);
    Append(glb, static_cast<std::uint32_t>(kHeaderBytes + (2 * kChunkHeaderBytes) + json.size() +
                                           bin_.size()));
    Append(glb, static_cast<std::uint32_t>(json.size()));
    Append(glb, kJsonChunk);
    const std::span<const std::byte> text = std::as_bytes(std::span(json));
    glb.insert(glb.end(), text.begin(), text.end());
    Append(glb, static_cast<std::uint32_t>(bin_.size()));
    Append(glb, kBinChunk);
    glb.insert(glb.end(), bin_.begin(), bin_.end());
    return glb;
}

}
