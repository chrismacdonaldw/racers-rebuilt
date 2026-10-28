#pragma once

#include "core/result.hpp"

namespace rr::platform {

/// Logs `error` and shows its message in a modal error box. Returns when the user closes it.
///
/// Works before SDL is initialized. The caller exits with a non-zero code afterwards.
void ReportFatalError(const Error& error);

}
