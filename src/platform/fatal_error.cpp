#include "platform/fatal_error.hpp"

#include "core/log.hpp"
#include "platform/message_box.hpp"

namespace rr::platform {

void ReportFatalError(const Error& error) {
    log::Error("{} error: {}", ToString(error.code), error.message);
    ShowErrorBox(error.message);
}

}
