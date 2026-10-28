#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace rr {

/// Category of a recoverable failure.
enum class ErrorCode : std::uint8_t {
    /// A file, folder or resource doesn't exist.
    NotFound,
    /// Reading or writing a file failed.
    Io,
    /// Data exists but is malformed or fails validation.
    InvalidData,
    /// An operating system or SDL call failed.
    Platform,
};

/// Recoverable failure with a message that can be shown to the user.
struct Error {
    ErrorCode code;
    std::string message;
};

/// Value or error returned by every fallible operation.
template <typename T>
using Result = std::expected<T, Error>;

/// Returns an error result for `code` carrying `message`.
[[nodiscard]] std::unexpected<Error> Fail(ErrorCode code, std::string message);

/// Returns the lowercase name of `code` for logs.
[[nodiscard]] std::string_view ToString(ErrorCode code);

}
