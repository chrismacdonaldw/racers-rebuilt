#include "overlay/overlay.hpp"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <string>

#include "overlay/setting_menus.hpp"
#include "platform/clock.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

namespace rr::overlay {
namespace {

constexpr std::string_view kDeveloperCategory = "Developer";

void Append(std::vector<std::string>& to, std::vector<std::string> from) {
    std::ranges::move(from, std::back_inserter(to));
}

double ToMilliseconds(Nanoseconds duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

void DrawStats(const DeveloperStats& stats, Nanoseconds overlayCost) {
    const double frameMs = stats.frameTime.count() * 1000.0;
    ImGui::Text("Frame time: %.2f ms (%.0f FPS)", frameMs, frameMs > 0.0 ? 1000.0 / frameMs : 0.0);
    ImGui::Text("Ticks this frame: %d", stats.ticksPerFrame);
    ImGui::Text("Overlay: %.3f ms", ToMilliseconds(overlayCost));
    ImGui::TextUnformatted(stats.startupSummary.data(),
                           stats.startupSummary.data() + stats.startupSummary.size());
    if (!stats.sceneStatus.empty()) {
        ImGui::TextUnformatted(stats.sceneStatus.data(),
                               stats.sceneStatus.data() + stats.sceneStatus.size());
    }
}

void DrawScenes(std::span<const SceneChoice> scenes) {
    if (!ImGui::BeginMenu("Scenes")) {
        return;
    }
    for (const SceneChoice& scene : scenes) {
        if (ImGui::MenuItem(scene.name.c_str())) {
            scene.open();
        }
    }
    ImGui::EndMenu();
}

}

Result<std::unique_ptr<Overlay>> Overlay::Create(const platform::Window& window,
                                                 render::Renderer& renderer) {
    std::unique_ptr<Overlay> overlay(new Overlay(renderer));
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplSDLGPU3_InitInfo info{.Device = renderer.Device(),
                                    .ColorTargetFormat = renderer.ColorFormat()};
    if (!ImGui_ImplSDL3_InitForSDLGPU(window.Handle())) {
        ImGui::DestroyContext();
        return Fail(ErrorCode::Platform, "can't start the overlay's input");
    }
    if (!ImGui_ImplSDLGPU3_Init(&info)) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return Fail(ErrorCode::Platform, "can't start the overlay's renderer");
    }
    return overlay;
}

Overlay::~Overlay() {
    renderer_->SetOverlay(nullptr);
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void Overlay::Toggle() {
    input_.SetOpen(!input_.IsOpen());
    hasFrame_ = false;
    renderer_->SetOverlay(input_.IsOpen() ? this : nullptr);
}

std::vector<std::string> Overlay::Build(settings::Registry& registry, const DeveloperStats& stats,
                                        std::span<const SceneChoice> scenes,
                                        const RacerTools& racerTools) {
    if (!input_.IsOpen()) {
        return {};
    }
    const Nanoseconds start = platform::Now();
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    std::vector<std::string> finished;
    if (ImGui::BeginMainMenuBar()) {
        const std::vector<SettingMenu> menus = GroupSettingMenus(registry.AllDeclarations());
        for (const SettingMenu& menu : menus) {
            if (menu.category != kDeveloperCategory &&
                ImGui::BeginMenu(std::string(menu.category).c_str())) {
                Append(finished, DrawSettingItems(registry, menu));
                ImGui::EndMenu();
            }
        }
        if (ImGui::BeginMenu(std::string(kDeveloperCategory).c_str())) {
            DrawStats(stats, lastFrameCost_);
            ImGui::Separator();
            DrawScenes(scenes);
            DrawRacerTools(racerTools);
            const auto developer =
                std::ranges::find(menus, kDeveloperCategory, &SettingMenu::category);
            if (developer != menus.end()) {
                Append(finished, DrawSettingItems(registry, *developer));
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::Render();
    hasFrame_ = true;
    frameCost_ = platform::Now() - start;
    return finished;
}

void Overlay::Prepare(SDL_GPUCommandBuffer* commands) {
    if (hasFrame_) {
        const Nanoseconds start = platform::Now();
        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
        frameCost_ += platform::Now() - start;
    }
}

void Overlay::Draw(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass) {
    if (hasFrame_) {
        const Nanoseconds start = platform::Now();
        ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commands, pass);
        lastFrameCost_ = frameCost_ + (platform::Now() - start);
        hasFrame_ = false;
    }
}

}
