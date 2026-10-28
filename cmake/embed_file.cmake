file(READ "${INPUT}" hex HEX)
string(LENGTH "${hex}" hexLength)
math(EXPR byteCount "${hexLength} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
file(WRITE "${OUTPUT}" "#include <cstdint>
#include <span>

namespace rr::render::shaders {
namespace {
constexpr std::uint8_t kBytes[${byteCount}] = {${bytes}};
}
extern const std::span<const std::uint8_t> ${SYMBOL};
const std::span<const std::uint8_t> ${SYMBOL}{kBytes};
}
")
