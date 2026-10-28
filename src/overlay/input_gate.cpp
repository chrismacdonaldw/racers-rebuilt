#include "overlay/input_gate.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_internal.h>

namespace rr::overlay {

void InputGate::Forward(const SDL_Event& event) const {
    if (isOpen_) {
        ImGui_ImplSDL3_ProcessEvent(&event);
    }
}

void InputGate::SetOpen(bool isOpen) {
    if (isOpen && !isOpen_) {
        ImGui::GetCurrentContext()->InputEventsQueue.resize(0);
        ImGuiIO& io = ImGui::GetIO();
        io.ClearInputKeys();
        io.ClearInputMouse();
    }
    isOpen_ = isOpen;
}

}
