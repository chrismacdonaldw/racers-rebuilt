#include <SDL3/SDL_main.h>

#include <cstdlib>

#include "app/application.hpp"
#include "core/result.hpp"
#include "platform/fatal_error.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    rr::Result<rr::app::Application> application = rr::app::Application::Create();
    if (!application) {
        rr::platform::ReportFatalError(application.error());
        return EXIT_FAILURE;
    }

    application->Run();
    return EXIT_SUCCESS;
}
