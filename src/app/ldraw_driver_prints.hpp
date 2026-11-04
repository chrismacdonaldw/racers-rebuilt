#pragma once

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "app/ldraw_driver_faces.hpp"
#include "assets/ldraw_part.hpp"
#include "assets/ldraw_prints.hpp"
#include "assets/vfs.hpp"
#include "mods/enhancement.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kLdrawPrintsSetting{
    .key = "enhancements.ldraw_prints",
    .defaultValue = false,
    .label = "LDraw driver prints",
    .category = "Enhancements/Visuals"};

inline constexpr std::uint32_t kTorsoPrintSizePx = 512;

/// `MENUDATA/PARTDB/<torso>.BMP` for a torso with an LDraw print, any case.
[[nodiscard]] std::optional<assets::LdrawPrint> PrintOfImagePath(std::string_view path);

/// Answers torso and face image reads with LDraw prints from `folder`, made once per part.
class LdrawDriverPrints final : public mods::Enhancement {
public:
    explicit LdrawDriverPrints(const std::filesystem::path& folder)
        : library_(folder), faces_(folder / "faces") {}

    [[nodiscard]] std::string_view Name() const override { return "LDraw driver prints"; }
    [[nodiscard]] bool ChangesGameplay() const override { return false; }

    void DeclareSettings(settings::Registry& registry) const override;
    void Attach(mods::HookRegistry& hooks, const settings::Registry& settings) override;

    /// Thread-safe. Empty when a mod also has `path`, or the image or part can't load.
    [[nodiscard]] std::optional<std::vector<std::byte>> ImageFile(const assets::LdrawPrint& print,
                                                                  const assets::Vfs& vfs,
                                                                  std::string_view path);

    /// Thread-safe. Empty when a mod also has `path`, or the face has no drawing.
    [[nodiscard]] std::optional<std::vector<std::byte>> FaceFile(const FaceImage& face,
                                                                 const assets::Vfs& vfs,
                                                                 std::string_view path);

private:
    std::mutex mutex_;
    assets::LdrawLibrary library_;
    std::unordered_map<std::string, std::optional<std::vector<std::byte>>> files_;
    LdrawDriverFaces faces_;
};

}
