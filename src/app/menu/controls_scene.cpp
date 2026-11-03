#include "app/menu/controls_scene.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <span>
#include <string_view>
#include <utility>

#include "app/menu/menu_page.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/race_bindings.hpp"
#include "app/ui_text/text_layout.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kFirstTitleLine = 0x17;
constexpr std::size_t kFirstActionLine = 0x69;
constexpr std::size_t kFinishLine = 0x1E;
constexpr std::size_t kDisabledState = 0;
constexpr std::size_t kListeningState = 3;
constexpr std::u16string_view kUnbound = u"........";
constexpr std::string_view kNameSeparator = ", ";
constexpr render::Color kDeviceColor{
    .r = 128.0F / 255.0F, .g = 128.0F / 255.0F, .b = 128.0F / 255.0F};

std::u16string ShownName(const std::vector<std::string>& names) {
    std::u16string shown;
    for (const std::string& name : names) {
        if (!shown.empty()) {
            shown.append(kNameSeparator.begin(), kNameSeparator.end());
        }
        for (const char letter : name) {
            shown.push_back(
                static_cast<char16_t>(std::toupper(static_cast<unsigned char>(letter))));
        }
    }
    return shown;
}

std::array<std::u16string, kControlRows> ShownNames(const ControlNames& names) {
    std::array<std::u16string, kControlRows> shown;
    for (std::size_t row = 0; row < kControlRows; ++row) {
        shown.at(row) = ShownName(names.at(row));
    }
    return shown;
}

}

Result<std::unique_ptr<ControlsScene>> ControlsScene::Create(render::Renderer& renderer,
                                                             const assets::Vfs& vfs,
                                                             std::size_t player, std::string back) {
    Result<ControlsLook> look = LoadControlsLook(renderer, vfs);
    if (!look) {
        return std::unexpected(std::move(look.error()));
    }
    return std::unique_ptr<ControlsScene>(
        new ControlsScene(renderer, vfs, std::move(*look), player, std::move(back)));
}

void ControlsScene::ApplySettings(const settings::Registry& settings) {
    static_cast<void>(text_.Apply(*renderer_, *vfs_, settings));
    for (const InputSet set : kInputSets) {
        const auto index = static_cast<std::size_t>(set);
        names_.at(index) = ReadControlNames(settings, set);
        shown_.at(index) = ShownNames(names_.at(index));
    }
    const bool isOpening = !chosen_.has_value();
    const PlayerSets& chosen = chosen_.emplace(ChosenPlayerSets(settings));
    if (isOpening) {
        ShowDevices(pads_, AssignInputSets(chosen, pads_.size()).at(player_));
    }
}

std::optional<std::string> ControlsScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

std::vector<SettingChange> ControlsScene::TakeSettingChanges() {
    return std::exchange(changes_, {});
}

void ControlsScene::HandleEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        ShowDevices(AttachedPads(), menu_.Set());
        return;
    }
    const std::optional<std::size_t> row = menu_.ListeningRow();
    if (!row) {
        return;
    }
    const std::optional<PressedControl> pressed = PressedControlOf(event);
    if (!pressed) {
        return;
    }
    const DeviceEntry& entry = Entry();
    const bool isOwnDevice =
        pressed->device == DeviceOf(entry.set) &&
        (pressed->device == ControlsDevice::Keyboard || pressed->pad == entry.pad);
    if (isOwnDevice && pressed->isRefused) {
        return;
    }
    if (isOwnDevice) {
        const ControlNames& names = names_.at(static_cast<std::size_t>(entry.set));
        std::ranges::move(RebindChanges(names, entry.set, *row, pressed->name),
                          std::back_inserter(changes_));
    }
    menu_.StopListening();
}

void ControlsScene::Tick(const sim::InputFrame& input) {
    switch (menu_.Tick(input)) {
        case ControlsRequest::Leave:
            Leave();
            return;
        case ControlsRequest::Listen:
        case ControlsRequest::None:
            return;
    }
}

void ControlsScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.clear();
    out.camera = {};
    out.simStateHash = menu_.StateHash();
}

void ControlsScene::Render([[maybe_unused]] const sim::RenderSnapshot& interpolated) {
    sprites_.clear();
    DrawPictures(std::span(&look_.backdrop, 1), sprites_);
    DrawBanner(text_, look_.banner, text_.Line(kFirstTitleLine + player_), look_.backdrop,
               look_.bannerTop, sprites_);
    DrawDevicePicker();
    DrawRows();
    DrawImageButton(text_, look_.finishButton,
                    menu_.IsFinishFocused() ? kFocusedState : kNormalState, text_.Line(kFinishLine),
                    look_.finish, sprites_);
    Result<void> drawn = renderer_->Draw({.sprites = sprites_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

ControlsScene::ControlsScene(render::Renderer& renderer, const assets::Vfs& vfs, ControlsLook look,
                             std::size_t player, std::string back)
    : renderer_(&renderer),
      vfs_(&vfs),
      look_(std::move(look)),
      player_(player),
      back_(std::move(back)) {
    ShowDevices(AttachedPads(), InputSet::Keys1);
}

const DeviceEntry& ControlsScene::Entry() const {
    return entries_.at(menu_.Entry());
}

void ControlsScene::ShowDevices(std::vector<AttachedPad> pads, InputSet set) {
    pads_ = std::move(pads);
    entries_ = DeviceEntries(pads_);
    std::vector<InputSet> sets;
    sets.reserve(entries_.size());
    for (const DeviceEntry& entry : entries_) {
        sets.push_back(entry.set);
    }
    menu_.SetEntries(std::move(sets), set);
}

void ControlsScene::Leave() {
    if (chosen_) {
        std::ranges::move(DeviceChanges(*chosen_, player_, menu_.Set(), pads_.size()),
                          std::back_inserter(changes_));
    }
    next_ = back_;
}

void ControlsScene::DrawDevicePicker() {
    const bool isFocused = menu_.IsDeviceFocused();
    DrawSelectorArrows(look_.arrows, look_.devicePicker, isFocused, sprites_);
    const UiImage& image = look_.deviceImages.at(static_cast<std::size_t>(Entry().image));
    sprites_.push_back({.texture = image.texture,
                        .bounds = SizedAt(look_.deviceImage, image),
                        .tint = isFocused ? render::Color{} : kDeviceColor});
}

void ControlsScene::DrawRows() {
    const std::array<std::u16string, kControlRows>& shownNames =
        shown_.at(static_cast<std::size_t>(menu_.Set()));
    const UiFont* labelFont = text_.Font(look_.label.font);
    for (std::size_t row = 0; row < kControlRows; ++row) {
        const MenuPoint label = look_.labels.at(row);
        if (labelFont != nullptr) {
            LayoutText(*labelFont, text_.Line(kFirstActionLine + row),
                       {.x = label.x, .y = label.y, .tint = look_.label.color}, sprites_);
        }
        const bool isListening = menu_.ListeningRow() == row;
        std::size_t state = menu_.FocusedRow() == row ? kFocusedState : kNormalState;
        if (isListening) {
            state = kListeningState;
        } else if (!menu_.IsRowEnabled(row)) {
            state = kDisabledState;
        }
        const std::u16string& shown = shownNames.at(row);
        const MenuPoint binding = look_.bindings.at(row);
        DrawLabel(text_, look_.bindingButton.at(state),
                  isListening || shown.empty() ? kUnbound : std::u16string_view(shown), binding.x,
                  binding.y, sprites_);
    }
}

}
