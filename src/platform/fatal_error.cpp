#include "platform/fatal_error.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_messagebox.h>

#include "core/log.hpp"

namespace rr::platform {

void ReportFatalError(const Error& error) {
    log::Error("{} error: {}", ToString(error.code), error.message);
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "racers-rebuilt", error.message.c_str(),
                                  nullptr)) {
        log::Error("can't show the error message box: {}", SDL_GetError());
    }
}

}
