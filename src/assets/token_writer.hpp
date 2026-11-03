#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/token_reader.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// Writes the token encoding that `TokenReader` reads, always in its plain forms.
class TokenWriter {
public:
    void Field(FieldId id);
    void Int(std::int32_t value);
    void Float(float value);

    /// Fails past 63 bytes or on a NUL, which the reader can't read back.
    [[nodiscard]] Result<void> String(std::string_view text);

    /// Writes `[ count ] {`. `Close` ends it.
    void OpenList(std::size_t count);
    void Close();

    [[nodiscard]] std::vector<std::byte> Take() && { return std::move(bytes_); }

private:
    void Code(std::uint8_t code);

    template <typename T>
    void Append(T value);

    std::vector<std::byte> bytes_;
};

}
