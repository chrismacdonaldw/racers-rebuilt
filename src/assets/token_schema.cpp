#include "assets/token_schema.hpp"

namespace rr::assets {

Result<void> ReadValue(TokenReader& reader, std::int32_t& value) {
    return reader.ReadInt().transform([&value](std::int32_t read) { value = read; });
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

}
