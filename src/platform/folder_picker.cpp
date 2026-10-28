#include "platform/folder_picker.hpp"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_properties.h>

#include <atomic>
#include <format>
#include <utility>

#include "core/utf8_path.hpp"

namespace rr::platform {
namespace {

constexpr int kPollIntervalMs = 10;

struct PickerState {
    std::atomic<bool> isDone = false;
    Result<std::optional<std::filesystem::path>> result = std::nullopt;
};

void OnFolderChosen(void* userdata, const char* const* folders, [[maybe_unused]] int filter) {
    auto* state = static_cast<PickerState*>(userdata);
    if (folders == nullptr) {
        state->result = Fail(ErrorCode::Platform,
                             std::format("can't show a folder picker: {}", SDL_GetError()));
    } else if (*folders != nullptr) {
        state->result = PathFromUtf8(*folders);
    }
    state->isDone.store(true, std::memory_order_release);
}

}

Result<std::optional<std::filesystem::path>> PickFolder(const std::string& title) {
    PickerState state;
    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetStringProperty(properties, SDL_PROP_FILE_DIALOG_TITLE_STRING, title.c_str());
    SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFOLDER, OnFolderChosen, &state, properties);
    while (!state.isDone.load(std::memory_order_acquire)) {
        SDL_WaitEventTimeout(nullptr, kPollIntervalMs);
    }
    SDL_DestroyProperties(properties);
    return std::move(state.result);
}

}
