#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "assets/face_drawing.hpp"
#include "assets/ldraw_part.hpp"

namespace rr::app {

struct FaceImage {
    std::string prefix;
    std::size_t expression = 0;
};

/// `MENUDATA/PARTDB/<prefix>_<expression>.BMP`, any case. The prefix comes back upper case.
[[nodiscard]] std::optional<FaceImage> FaceOfImagePath(std::string_view path);

/// Reads `<prefix>.face` files in `folder` and draws all six expressions once per prefix.
class LdrawDriverFaces {
public:
    explicit LdrawDriverFaces(std::filesystem::path folder) : folder_(std::move(folder)) {}

    /// PNG bytes. Empty when the prefix has no face file or it can't be drawn.
    [[nodiscard]] std::optional<std::vector<std::byte>> ImageFile(const FaceImage& face,
                                                                  assets::LdrawLibrary& library);

private:
    using Expressions = std::array<std::vector<std::byte>, assets::kFaceExpressionCount>;

    std::filesystem::path folder_;
    std::unordered_map<std::string, std::optional<Expressions>> faces_;
};

}
