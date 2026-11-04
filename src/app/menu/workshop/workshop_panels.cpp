#include "app/menu/workshop/workshop_panels.hpp"

#include <algorithm>
#include <format>
#include <string_view>
#include <utility>

#include "app/menu/car_build_look.hpp"
#include "app/menu/edit_car_look.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/stand_scene.hpp"

namespace rr::app {
namespace {

constexpr std::array kActions{PanelAction::Done, PanelAction::Undo,   PanelAction::Redo,
                              PanelAction::Turn, PanelAction::Delete, PanelAction::Frame};

Result<UiImage> SetIcon(const MenuFiles& files, MenuImageCache& images, std::string_view style) {
    const assets::ImageStyle* image = files.FindImageStyle(style);
    if (image == nullptr) {
        return Fail(ErrorCode::NotFound, std::format("there's no image style {}", style));
    }
    return images.Upload(image->image);
}

bool IsSame(const render::Rect& a, const render::Rect& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

}

Result<std::vector<UiImage>> LoadSetIcons(render::Renderer& renderer, const assets::Vfs& vfs) {
    Result<MenuFiles> files = ReadMenuFiles(vfs, kCarBuildScreen);
    Result<void> added =
        files.and_then([&](MenuFiles& read) { return AddBuilderImages(vfs, read); });
    if (!added) {
        return std::unexpected(std::move(added.error()));
    }
    MenuImageCache images(renderer, vfs, *files);
    std::vector<UiImage> icons;
    for (const std::string_view style : kSetImageStyles) {
        Result<UiImage> icon = SetIcon(*files, images, style);
        if (!icon) {
            return std::unexpected(std::move(icon.error()));
        }
        icons.push_back(*icon);
    }
    return icons;
}

WorkshopPanels::WorkshopPanels(const assets::PieceCatalog& catalog, std::vector<std::size_t> sets,
                               std::vector<UiImage> setIcons)
    : catalog_(&catalog),
      sets_(std::move(sets)),
      setIcons_(std::move(setIcons)),
      layout_(LayOutWorkshop({}, sets_.size())),
      drawer_(layout_.tiles),
      swatches_(catalog.colours.size(), layout_.colours),
      light_{MenuSceneLight()} {
    ShowTab(0);
}

void WorkshopPanels::Fit(const render::Rect& window) {
    if (IsSame(window, window_)) {
        return;
    }
    window_ = window;
    LayOut();
}

void WorkshopPanels::LayOut() {
    layout_ = LayOutWorkshop(window_, sets_.size());
    const std::size_t firstRow = drawer_.FirstRow();
    const std::optional<std::uint8_t> chosen = swatches_.Chosen();
    drawer_ = PiecePalette(layout_.tiles);
    swatches_ = ColourSwatches(catalog_->colours.size(), layout_.colours);
    ChooseColour(chosen);
    ShowTab(tab_);
    drawer_.Scroll(static_cast<int>(firstRow));
    tiles_.resize(layout_.tiles.columns * layout_.tiles.rows);
    hovered_.reset();
}

void WorkshopPanels::OfferExtras(bool offers) {
    const std::size_t extra = catalog_->ExtraSet();
    const bool isOffered = std::ranges::contains(sets_, extra);
    if (offers == isOffered || catalog_->extras.empty()) {
        return;
    }
    if (offers) {
        sets_.push_back(extra);
    } else {
        std::erase(sets_, extra);
        tab_ = std::min(tab_, sets_.empty() ? 0 : sets_.size() - 1);
    }
    LayOut();
}

PanelTick WorkshopPanels::Tick(const sim::MenuPointer& pointer) {
    const float x = pointer.x;
    const float y = pointer.y;
    PanelTick tick{.isOver = Covers(x, y)};
    hovered_.reset();
    if (const std::optional<std::size_t> entry = drawer_.EntryAt(x, y)) {
        hovered_ = drawer_.TileOf(*entry);
        if (pointer.LeftPressed()) {
            tick.grabbed = drawer_.Entries().at(*entry);
        }
    } else if (const std::optional<std::size_t> tab = TabAt(x, y)) {
        hovered_ = TabOf(layout_, *tab);
        if (pointer.LeftPressed()) {
            ShowTab(*tab);
        }
    } else if (const std::optional<PanelAction> action = ActionAt(x, y)) {
        hovered_ = AreaOf(*action);
        if (pointer.LeftPressed()) {
            tick.action = action;
        }
    } else if (pointer.LeftPressed()) {
        tick.swatch = swatches_.ColourAt(x, y);
    }
    if (Contains(layout_.drawer, x, y) && pointer.wheel != 0.0F) {
        drawer_.Scroll(pointer.wheel > 0.0F ? -1 : 1);
    }
    return tick;
}

bool WorkshopPanels::Covers(float x, float y) const {
    return Contains(layout_.drawer, x, y) || Contains(layout_.colours, x, y) ||
           ActionAt(x, y).has_value();
}

void WorkshopPanels::ChooseColour(std::optional<std::uint8_t> colour) {
    if (colour) {
        swatches_.Choose(*colour);
    } else {
        swatches_.Forget();
    }
}

std::uint8_t WorkshopPanels::ColourOf(const PaletteEntry& entry) const {
    return swatches_.Chosen().value_or(catalog_->Choices(entry.set)[entry.choice].colour);
}

void WorkshopPanels::ShowTab(std::size_t tab) {
    if (tab >= sets_.size()) {
        return;
    }
    tab_ = tab;
    drawer_.Show(*catalog_, sets_[tab]);
}

render::Rect WorkshopPanels::AreaOf(PanelAction action) const {
    if (action == PanelAction::Done) {
        return layout_.done;
    }
    return layout_.tools.at(static_cast<std::size_t>(action) - 1);
}

std::optional<PanelAction> WorkshopPanels::ActionAt(float x, float y) const {
    for (const PanelAction action : kActions) {
        if (Contains(AreaOf(action), x, y)) {
            return action;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> WorkshopPanels::TabAt(float x, float y) const {
    for (std::size_t tab = 0; tab < std::min(sets_.size(), kTabColumns * kTabRows); ++tab) {
        if (Contains(TabOf(layout_, tab), x, y)) {
            return tab;
        }
    }
    return std::nullopt;
}

}
