#pragma once

#include <string>

namespace rr::platform {

/// Blocks until the user closes the box. Works before SDL is initialized.
void ShowErrorBox(const std::string& message);

}
