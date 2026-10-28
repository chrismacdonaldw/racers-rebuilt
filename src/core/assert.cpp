#include "core/assert.hpp"

#include <cstdlib>

#include "core/log.hpp"

namespace rr {

void FailCheck(std::string_view expression, std::source_location location) {
    log::Error("check failed: {} at {}:{} in {}", expression, location.file_name(), location.line(),
               location.function_name());
    log::CloseFile();
    std::abort();
}

}
