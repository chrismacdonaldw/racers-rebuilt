#include "platform/process.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_process.h>

#include <format>
#include <vector>

namespace rr::platform {

Result<void> LaunchDetached(std::span<const std::string> command) {
    std::vector<const char*> argv;
    argv.reserve(command.size() + 1);
    for (const std::string& part : command) {
        argv.push_back(part.c_str());
    }
    argv.push_back(nullptr);
    SDL_Process* process = SDL_CreateProcess(argv.data(), false);
    if (process == nullptr) {
        return Fail(ErrorCode::Platform, std::format("can't relaunch: {}", SDL_GetError()));
    }
    SDL_DestroyProcess(process);
    return {};
}

}
