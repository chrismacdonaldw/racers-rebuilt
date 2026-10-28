#include "assets/token_schema.hpp"

#include <utility>

namespace rr::assets {

Result<void> ReadValue(TokenReader& reader, std::int32_t& value) {
    return reader.ReadInt().transform([&value](std::int32_t read) { value = read; });
}

Result<void> ReadValue(TokenReader& reader, std::uint8_t& value) {
    return reader.ReadInt().and_then([&value](std::int32_t read) -> Result<void> {
        if (!std::in_range<std::uint8_t>(read)) {
            return Fail(ErrorCode::InvalidData, std::format("{} is outside 0 to 255", read));
        }
        value = static_cast<std::uint8_t>(read);
        return {};
    });
}

Result<void> ReadValue(TokenReader& reader, float& value) {
    return reader.ReadFloat().transform([&value](float read) { value = read; });
}

Result<void> ReadValue(TokenReader& reader, std::string& value) {
    return reader.ReadString().transform([&value](std::string_view read) { value = read; });
}

Result<void> ReadValue([[maybe_unused]] TokenReader& reader, bool& value) {
    value = true;
    return {};
}

Result<void> ExpectField(TokenReader& reader, FieldId id) {
    return reader.Expect(TokenKind::Field).and_then([id](const Token& field) -> Result<void> {
        if (field.field != id) {
            return Fail(ErrorCode::InvalidData,
                        std::format("expected field 0x{:02X} at offset {}, found 0x{:02X}", id,
                                    field.offset, field.field));
        }
        return {};
    });
}

}
