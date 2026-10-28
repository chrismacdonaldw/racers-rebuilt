#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace rr {

enum class ErrorCode : std::uint8_t {
    NotFound,
    Io,
    InvalidData,
    Platform,
};

/// `message` is written to be shown to the user.
struct Error {
    ErrorCode code;
    std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

[[nodiscard]] std::unexpected<Error> Fail(ErrorCode code, std::string message);

[[nodiscard]] std::string_view ToString(ErrorCode code);

}
