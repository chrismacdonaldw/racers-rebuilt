#include "platform/win32/unique_handle.hpp"

#include <windows.h>

namespace rr::platform {

void HandleCloser::operator()(void* handle) const {
    CloseHandle(handle);
}

}
