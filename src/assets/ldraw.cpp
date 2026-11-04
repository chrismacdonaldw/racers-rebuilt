#include "assets/ldraw.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <system_error>
#include <utility>

namespace rr::assets {
namespace {

constexpr std::size_t kMatrixNumbers = 12;
constexpr std::size_t kCornerNumbers = 3;
constexpr int kHexBase = 16;
constexpr std::string_view kHexPrefix = "0x";
constexpr std::string_view kWhitespace = " \t\r";

std::vector<std::string_view> Tokens(std::string_view line) {
    std::vector<std::string_view> tokens;
    std::size_t start = line.find_first_not_of(kWhitespace);
    while (start != std::string_view::npos) {
        const std::size_t end = line.find_first_of(kWhitespace, start);
        tokens.push_back(line.substr(start, end - start));
        start = end == std::string_view::npos ? end : line.find_first_not_of(kWhitespace, end);
    }
    return tokens;
}

std::optional<float> ToFloat(std::string_view token) {
    float value = 0.0F;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error != std::errc{} || end != token.data() + token.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::int32_t> ToColour(std::string_view token) {
    int base = 10;
    if (token.starts_with(kHexPrefix)) {
        token.remove_prefix(kHexPrefix.size());
        base = kHexBase;
    }
    std::int32_t value = 0;
    const auto [end, error] =
        std::from_chars(token.data(), token.data() + token.size(), value, base);
    if (error != std::errc{} || end != token.data() + token.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::vector<float>> ToFloats(std::span<const std::string_view> tokens) {
    std::vector<float> values;
    values.reserve(tokens.size());
    for (const std::string_view token : tokens) {
        const std::optional<float> value = ToFloat(token);
        if (!value) {
            return std::nullopt;
        }
        values.push_back(*value);
    }
    return values;
}

Vector3 PointAt(std::span<const float> values, std::size_t first) {
    return {.x = values[first], .y = values[first + 1], .z = values[first + 2]};
}

std::string JoinedName(std::span<const std::string_view> tokens) {
    std::string name;
    for (const std::string_view token : tokens) {
        if (!name.empty()) {
            name += ' ';
        }
        name += token;
    }
    return name;
}

class LdrawReader {
public:
    [[nodiscard]] Result<LdrawFile> Read(std::string_view text) {
        std::size_t lineNumber = 0;
        while (!text.empty()) {
            const std::size_t end = text.find('\n');
            const std::vector<std::string_view> tokens = Tokens(text.substr(0, end));
            text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
            ++lineNumber;
            if (!tokens.empty() && !ReadLine(tokens)) {
                return Fail(ErrorCode::InvalidData,
                            std::format("LDraw line {} is malformed", lineNumber));
            }
        }
        return std::move(file_);
    }

private:
    [[nodiscard]] bool ReadLine(std::span<const std::string_view> tokens) {
        const std::string_view type = tokens[0];
        if (type == "0") {
            ReadMeta(tokens.subspan(1));
            return true;
        }
        if (type == "1") {
            return ReadReference(tokens.subspan(1));
        }
        if (type == "3" || type == "4") {
            return ReadFace(tokens.subspan(1), type == "3" ? 3 : 4);
        }
        return true;
    }

    void ReadMeta(std::span<const std::string_view> tokens) {
        if (tokens.empty()) {
            return;
        }
        if (tokens[0] == "!LICENSE") {
            file_.licence = JoinedName(tokens.subspan(1));
            return;
        }
        if (tokens[0] != "BFC") {
            return;
        }
        for (const std::string_view word : tokens.subspan(1)) {
            if (word == "CW") {
                isClockwise_ = true;
            } else if (word == "CCW") {
                isClockwise_ = false;
            } else if (word == "INVERTNEXT") {
                invertsNext_ = true;
            }
        }
    }

    [[nodiscard]] bool ReadReference(std::span<const std::string_view> tokens) {
        if (tokens.size() < 2 + kMatrixNumbers) {
            return false;
        }
        const std::optional<std::int32_t> colour = ToColour(tokens[0]);
        const std::optional<std::vector<float>> values =
            ToFloats(tokens.subspan(1, kMatrixNumbers));
        if (!colour || !values) {
            return false;
        }
        LdrawMatrix matrix{.position = PointAt(*values, 0)};
        for (std::size_t row = 0; row < matrix.rows.size(); ++row) {
            matrix.rows.at(row) = PointAt(*values, kCornerNumbers * (row + 1));
        }
        file_.references.push_back({.colour = *colour,
                                    .matrix = matrix,
                                    .file = JoinedName(tokens.subspan(1 + kMatrixNumbers)),
                                    .isInverted = std::exchange(invertsNext_, false)});
        return true;
    }

    [[nodiscard]] bool ReadFace(std::span<const std::string_view> tokens, std::uint8_t corners) {
        if (tokens.size() != 1 + (corners * kCornerNumbers)) {
            return false;
        }
        const std::optional<std::int32_t> colour = ToColour(tokens[0]);
        const std::optional<std::vector<float>> values = ToFloats(tokens.subspan(1));
        if (!colour || !values) {
            return false;
        }
        LdrawFace face{.colour = *colour, .cornerCount = corners};
        for (std::size_t corner = 0; corner < corners; ++corner) {
            face.corners.at(corner) = PointAt(*values, corner * kCornerNumbers);
        }
        if (isClockwise_) {
            std::reverse(face.corners.begin() + 1, face.corners.begin() + corners);
        }
        file_.faces.push_back(face);
        return true;
    }

    LdrawFile file_;
    bool isClockwise_ = false;
    bool invertsNext_ = false;
};

}

Vector3 Apply(const LdrawMatrix& matrix, const Vector3& point) {
    return matrix.position + Vector3{.x = Dot(matrix.rows[0], point),
                                     .y = Dot(matrix.rows[1], point),
                                     .z = Dot(matrix.rows[2], point)};
}

LdrawMatrix Compose(const LdrawMatrix& outer, const LdrawMatrix& inner) {
    LdrawMatrix composed{.position = Apply(outer, inner.position)};
    const Vector3 columnX{.x = inner.rows[0].x, .y = inner.rows[1].x, .z = inner.rows[2].x};
    const Vector3 columnY{.x = inner.rows[0].y, .y = inner.rows[1].y, .z = inner.rows[2].y};
    const Vector3 columnZ{.x = inner.rows[0].z, .y = inner.rows[1].z, .z = inner.rows[2].z};
    for (std::size_t row = 0; row < composed.rows.size(); ++row) {
        const Vector3& outerRow = outer.rows.at(row);
        composed.rows.at(row) = {
            .x = Dot(outerRow, columnX), .y = Dot(outerRow, columnY), .z = Dot(outerRow, columnZ)};
    }
    return composed;
}

float Determinant(const LdrawMatrix& matrix) {
    return Dot(matrix.rows[0], Cross(matrix.rows[1], matrix.rows[2]));
}

Result<LdrawFile> ParseLdraw(std::string_view text) {
    return LdrawReader().Read(text);
}

}
