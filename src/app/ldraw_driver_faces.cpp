#include "app/ldraw_driver_faces.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <span>
#include <system_error>
#include <utility>

#include "assets/driver_parts.hpp"
#include "assets/game_path.hpp"
#include "assets/ldraw_head.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "platform/image_file.hpp"
#include "platform/mapped_file.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPartFolder = "MENUDATA/PARTDB/";
constexpr std::string_view kImageExtension = ".BMP";
constexpr std::string_view kFaceExtension = ".face";
constexpr std::size_t kMaxPrefixLength = 8;

bool IsPrefix(std::string_view text) {
    return !text.empty() && text.size() <= kMaxPrefixLength &&
           std::ranges::all_of(
               text, [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; });
}

std::optional<std::size_t> ExpressionOf(std::string_view suffix) {
    const std::string upper = assets::ToUpperAscii(suffix);
    for (std::size_t expression = 0; expression < assets::kFaceExpressionCount; ++expression) {
        if (assets::ToUpperAscii(assets::FaceMaterial("", static_cast<std::int32_t>(expression))) ==
            upper) {
            return expression;
        }
    }
    return std::nullopt;
}

Result<std::string> ReadText(const std::filesystem::path& path) {
    return platform::MappedFile::Open(path).transform([](const platform::MappedFile& mapped) {
        const std::span<const std::byte> bytes = mapped.Bytes();
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    });
}

Result<assets::ImageAsset> BaseOf(const assets::FaceDrawing& drawing,
                                  assets::LdrawLibrary& library) {
    if (drawing.ldrawPart.empty()) {
        return assets::PlainFace(drawing.plain);
    }
    return library.LoadPart(drawing.ldrawPart).transform(assets::DrawHeadPrint);
}

Result<std::array<std::vector<std::byte>, assets::kFaceExpressionCount>> DrawnFiles(
    const std::filesystem::path& path, assets::LdrawLibrary& library) {
    Result<assets::FaceDrawing> drawing = ReadText(path).and_then(
        [](const std::string& text) { return assets::ParseFaceDrawing(text); });
    if (!drawing) {
        return std::unexpected(drawing.error());
    }
    Result<assets::ImageAsset> base = BaseOf(*drawing, library);
    if (!base) {
        return std::unexpected(base.error());
    }
    std::array<std::vector<std::byte>, assets::kFaceExpressionCount> files;
    const auto faces = assets::DrawFaces(*drawing, *base);
    for (std::size_t expression = 0; expression < faces.size(); ++expression) {
        const assets::ImageAsset& face = faces.at(expression);
        Result<std::vector<std::byte>> file =
            platform::EncodePng(face.widthPx, face.heightPx, face.rgba);
        if (!file) {
            return std::unexpected(file.error());
        }
        files.at(expression) = std::move(*file);
    }
    return files;
}

}

std::optional<FaceImage> FaceOfImagePath(std::string_view path) {
    if (!path.starts_with(kPartFolder) || !path.ends_with(kImageExtension)) {
        return std::nullopt;
    }
    path.remove_prefix(kPartFolder.size());
    path.remove_suffix(kImageExtension.size());
    const std::size_t split = path.rfind('_');
    if (split == std::string_view::npos || !IsPrefix(path.substr(0, split))) {
        return std::nullopt;
    }
    return ExpressionOf(path.substr(split + 1)).transform([&](std::size_t expression) {
        return FaceImage{.prefix = assets::ToUpperAscii(path.substr(0, split)),
                         .expression = expression};
    });
}

std::optional<std::vector<std::byte>> LdrawDriverFaces::ImageFile(const FaceImage& face,
                                                                  assets::LdrawLibrary& library) {
    const auto [found, isNew] = faces_.try_emplace(face.prefix);
    if (isNew) {
        const std::filesystem::path path =
            folder_ / PathFromUtf8(std::format("{}{}", face.prefix, kFaceExtension));
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error)) {
            auto files = DrawnFiles(path, library);
            if (files) {
                found->second = std::move(*files);
            } else {
                log::Warn("Keeping face {} because its LDraw drawing can't be drawn: {}",
                          face.prefix, files.error().message);
            }
        }
    }
    return found->second.transform(
        [&](const Expressions& files) { return files.at(face.expression); });
}

}
