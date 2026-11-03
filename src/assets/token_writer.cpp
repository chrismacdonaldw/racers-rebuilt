#include "assets/token_writer.hpp"

#include <array>
#include <bit>
#include <format>

namespace rr::assets {
namespace {

static_assert(std::endian::native == std::endian::little);

constexpr std::uint8_t kString = 0x02;
constexpr std::uint8_t kFloat = 0x03;
constexpr std::uint8_t kInt = 0x04;
constexpr std::uint8_t kOpenBlock = 0x05;
constexpr std::uint8_t kCloseBlock = 0x06;
constexpr std::uint8_t kOpenBracket = 0x07;
constexpr std::uint8_t kCloseBracket = 0x08;
constexpr std::uint8_t kWideField = 0x13;
constexpr FieldId kFirstShortField = 0x27;
constexpr FieldId kLastShortField = 0xFF;
constexpr std::size_t kMaxStringBytes = 63;

}

void TokenWriter::Field(FieldId id) {
    if (id >= kFirstShortField && id <= kLastShortField) {
        Code(static_cast<std::uint8_t>(id));
        return;
    }
    Code(kWideField);
    Append(id);
}

void TokenWriter::Int(std::int32_t value) {
    Code(kInt);
    Append(value);
}

void TokenWriter::Float(float value) {
    Code(kFloat);
    Append(value);
}

Result<void> TokenWriter::String(std::string_view text) {
    if (text.size() > kMaxStringBytes || text.contains('\0')) {
        return Fail(ErrorCode::InvalidData,
                    std::format("\"{}\" can't be written: names are at most {} bytes, with no NUL",
                                text, kMaxStringBytes));
    }
    Code(kString);
    for (const char letter : text) {
        bytes_.push_back(static_cast<std::byte>(letter));
    }
    bytes_.push_back(std::byte{0});
    return {};
}

void TokenWriter::OpenList(std::size_t count) {
    Code(kOpenBracket);
    Int(static_cast<std::int32_t>(count));
    Code(kCloseBracket);
    Code(kOpenBlock);
}

void TokenWriter::OpenBlock() {
    Code(kOpenBlock);
}

void TokenWriter::Close() {
    Code(kCloseBlock);
}

void TokenWriter::Code(std::uint8_t code) {
    bytes_.push_back(std::byte{code});
}

template <typename T>
void TokenWriter::Append(T value) {
    const auto raw = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    bytes_.insert(bytes_.end(), raw.begin(), raw.end());
}

}
