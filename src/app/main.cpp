#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "app/application.hpp"
#include "app/game_boot.hpp"
#include "app/image_dump.hpp"
#include "core/result.hpp"
#include "core/utf8_path.hpp"
#include "platform/fatal_error.hpp"

namespace {

rr::Result<void> Run(std::span<const std::string_view> args) {
    rr::Result<rr::app::GameBoot> boot = rr::app::BootGame(args);
    if (!boot) {
        return std::unexpected(std::move(boot.error()));
    }
    if (boot->commandLine.imageDumpFolder) {
        return rr::app::DumpImages(boot->vfs, rr::PathFromUtf8(*boot->commandLine.imageDumpFolder));
    }
    rr::Result<rr::app::Application> application = rr::app::Application::Create(std::move(*boot));
    if (!application) {
        return std::unexpected(std::move(application.error()));
    }
    return application->Run();
}

}

int main(int argc, char* argv[]) {
    const std::span<char*> argSpan(argv, static_cast<std::size_t>(argc));
    const std::vector<std::string_view> args(argSpan.begin() + 1, argSpan.end());
    const rr::Result<void> ran = Run(args);
    if (!ran) {
        rr::platform::ReportFatalError(ran.error());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
