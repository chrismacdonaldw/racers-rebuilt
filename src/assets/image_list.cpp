#include "assets/image_list.hpp"

#include <array>

#include "assets/color_key_field.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kImageField = 0x27;

constexpr std::array kImageFields{
    FlagField<&ImageListEntry::fileKind, TextureFileKind::Bmp>(0x29),
    Field<&ImageListEntry::colorKey>(0x2B),
};

}

Result<std::vector<ImageListEntry>> ParseImageList(std::span<const std::byte> bytes) {
    return ParseNamedLibrary<ImageListEntry>(bytes, kImageField, kImageFields);
}

}
