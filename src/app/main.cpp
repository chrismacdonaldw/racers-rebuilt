#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <span>
#include <string_view>
#include <vector>

#include "app/application.hpp"
#include "core/result.hpp"
#include "platform/fatal_error.hpp"

int main(int argc, char* argv[]) {
    const std::span<char*> argSpan(argv, static_cast<std::size_t>(argc));
    const std::vector<std::string_view> args(argSpan.begin() + 1, argSpan.end());
    rr::Result<rr::app::Application> application = rr::app::Application::Create(args);
    if (!application) {
        rr::platform::ReportFatalError(application.error());
        return EXIT_FAILURE;
    }

    const rr::Result<void> ran = application->Run();
    if (!ran) {
        rr::platform::ReportFatalError(ran.error());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
