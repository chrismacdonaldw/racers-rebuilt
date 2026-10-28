#include "core/result.hpp"

#include <utility>

namespace rr {

std::unexpected<Error> Fail(ErrorCode code, std::string message) {
    return std::unexpected(Error{.code = code, .message = std::move(message)});
}

std::string_view ToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::NotFound:
            return "not found";
        case ErrorCode::Io:
            return "i/o";
        case ErrorCode::InvalidData:
            return "invalid data";
        case ErrorCode::Platform:
            return "platform";
    }
    return "unknown";
}

}
