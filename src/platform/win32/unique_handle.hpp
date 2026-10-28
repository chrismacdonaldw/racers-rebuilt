#pragma once

#include <memory>

namespace rr::platform {

struct HandleCloser {
    void operator()(void* handle) const;
};

/// Owns a Win32 `HANDLE` released with `CloseHandle`. Never give it `INVALID_HANDLE_VALUE`.
using UniqueHandle = std::unique_ptr<void, HandleCloser>;

}
