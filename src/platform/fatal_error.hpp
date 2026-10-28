#pragma once

#include "core/result.hpp"

namespace rr::platform {

/// Blocks until the user closes the message box. Works before SDL is initialized.
void ReportFatalError(const Error& error);

}
