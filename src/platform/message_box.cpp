#include "platform/message_box.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_messagebox.h>

#include "core/log.hpp"

namespace rr::platform {

void ShowErrorBox(const std::string& message) {
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "racers-rebuilt", message.c_str(),
                                  nullptr)) {
        log::Error("can't show the error message box: {}", SDL_GetError());
    }
}

}
