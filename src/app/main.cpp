#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "app/application.hpp"
#include "app/circuit_awards.hpp"
#include "app/game_boot.hpp"
#include "app/image_dump.hpp"
#include "app/model_export.hpp"
#include "app/restart.hpp"
#include "app/save_import.hpp"
#include "app/texture_pack.hpp"
#include "core/result.hpp"
#include "core/utf8_path.hpp"
#include "platform/fatal_error.hpp"

namespace {

rr::Result<void> Run(std::span<const std::string_view> argv) {
    rr::Result<rr::app::GameBoot> boot = rr::app::BootGame(argv.subspan(1));
    if (!boot) {
        return std::unexpected(std::move(boot.error()));
    }
    boot->relaunch = rr::app::RelaunchCommand(argv);
    if (boot->commandLine.importSaveFile) {
        const rr::Result<bool> imported = rr::app::ImportOriginalSave(
            rr::PathFromUtf8(*boot->commandLine.importSaveFile),
            boot->settingsFile.parent_path() / rr::app::kProgressFileName, boot->vfs);
        if (!imported) {
            return std::unexpected(imported.error());
        }
    }
    if (boot->commandLine.imageDumpFolder) {
        return rr::app::DumpImages(boot->vfs, rr::PathFromUtf8(*boot->commandLine.imageDumpFolder));
    }
    if (boot->commandLine.texturePackFolder) {
        return rr::app::DumpTexturePack(boot->vfs, boot->extensions.gameLayers - 1,
                                        rr::PathFromUtf8(*boot->commandLine.texturePackFolder));
    }
    if (boot->commandLine.gltfExportFolder) {
        return rr::app::ExportGameModels(boot->vfs, boot->extensions.gameLayers - 1,
                                         rr::PathFromUtf8(*boot->commandLine.gltfExportFolder));
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
    const std::vector<std::string_view> args(argSpan.begin(), argSpan.end());
    const rr::Result<void> ran = Run(args);
    if (!ran) {
        rr::platform::ReportFatalError(ran.error());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
