#include "overlay/input_gate.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>

namespace rr::overlay {

void InputGate::Forward(const SDL_Event& event) const {
    if (isOpen_) {
        ImGui_ImplSDL3_ProcessEvent(&event);
    }
}

void InputGate::SetOpen(bool isOpen) {
    if (isOpen && !isOpen_) {
        ImGuiIO& io = ImGui::GetIO();
        io.ClearEventsQueue();
        io.ClearInputKeys();
        io.ClearInputMouse();
    }
    isOpen_ = isOpen;
}

}
