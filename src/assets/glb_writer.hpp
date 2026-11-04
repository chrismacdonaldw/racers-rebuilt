#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rr::assets {

[[nodiscard]] std::string JsonString(std::string_view text);

class GlbWriter {
public:
    [[nodiscard]] std::size_t AddFloats(std::span<const float> values, std::string_view type,
                                        bool hasBounds = false);
    [[nodiscard]] std::size_t AddJoints(std::span<const std::uint16_t> values);
    [[nodiscard]] std::size_t AddIndices(std::span<const std::uint32_t> values);

    /// `fields` are the JSON members after `asset`, without the buffer ones, comma-joined.
    [[nodiscard]] std::vector<std::byte> Build(std::string_view fields) const;

private:
    std::size_t Add(std::span<const std::byte> bytes, std::size_t count, int component,
                    std::string_view type, std::string_view bounds, std::string_view target);

    std::vector<std::byte> bin_;
    std::string views_;
    std::string accessors_;
    std::size_t count_ = 0;
};

}
