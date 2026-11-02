#include "app/menu/award_scene.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <string_view>
#include <utility>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_scenes.hpp"
#include "assets/game_path.hpp"
#include "assets/image_list.hpp"
#include "core/log.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "MAINMENU";
constexpr std::string_view kTrophyList = "DRVRLICE.IDB";
constexpr std::array<std::string_view, 3> kTrophies{"gtrophy", "strophy", "btrophy"};

Result<std::optional<UiImage>> LoadTrophy(render::Renderer& renderer, const assets::Vfs& vfs,
                                          sim::CircuitAward award) {
    const auto index = static_cast<std::size_t>(award);
    if (index >= kTrophies.size()) {
        return std::optional<UiImage>();
    }
    Result<std::vector<assets::ImageListEntry>> entries =
        vfs.Read(assets::JoinGamePath(kMenuFolder, kTrophyList))
            .and_then(
                [](const assets::FileData& file) { return assets::ParseImageList(file.Bytes()); });
    if (!entries) {
        return std::unexpected(std::move(entries.error()));
    }
    const auto entry = std::ranges::find(*entries, kTrophies[index], &assets::ImageListEntry::name);
    if (entry == entries->end()) {
        return Fail(ErrorCode::NotFound,
                    std::format("{} has no image {}", kTrophyList, kTrophies[index]));
    }
    return LoadUiImage(renderer, vfs, kMenuFolder, *entry).transform([](const UiImage& image) {
        return std::optional(image);
    });
}

}

Result<std::unique_ptr<AwardScene>> AwardScene::Create(render::Renderer& renderer,
                                                       const assets::Vfs& vfs,
                                                       sim::CircuitAward award) {
    Result<MenuPage> page = ReadMenuFiles(vfs, kScreen).and_then([&](const MenuFiles& files) {
        return BuildMenuPage(renderer, vfs, files, std::span<const ButtonLine>{});
    });
    Result<std::optional<UiImage>> trophy =
        page.and_then([&](const MenuPage&) { return LoadTrophy(renderer, vfs, award); });
    if (!trophy) {
        return std::unexpected(std::move(trophy.error()));
    }
    return std::unique_ptr<AwardScene>(
        new AwardScene(renderer, std::move(page->pictures), *trophy));
}

std::optional<std::string> AwardScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

void AwardScene::Tick(const sim::InputFrame& input) {
    if (input.WasPressed(sim::MenuAction::Confirm) || input.WasPressed(sim::MenuAction::Back)) {
        next_ = std::string(kMainMenuScene);
    }
}

void AwardScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = 0;
}

void AwardScene::Render([[maybe_unused]] const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    DrawPictures(pictures_, sprites_);
    if (trophy_) {
        const auto width = static_cast<float>(trophy_->widthPx);
        const auto height = static_cast<float>(trophy_->heightPx);
        sprites_.push_back({.texture = trophy_->texture,
                            .bounds = {.x = (render::kUiWidth - width) / 2.0F,
                                       .y = (render::kUiHeight - height) / 2.0F,
                                       .width = width,
                                       .height = height}});
    }
    Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

AwardScene::AwardScene(render::Renderer& renderer, std::vector<MenuPicture> pictures,
                       std::optional<UiImage> trophy)
    : renderer_(&renderer), pictures_(std::move(pictures)), trophy_(trophy) {}

}
