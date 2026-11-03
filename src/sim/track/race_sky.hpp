#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "assets/event_table.hpp"
#include "assets/sky_file.hpp"
#include "core/duration.hpp"

namespace rr::sim {

struct SkyColors {
    assets::Rgb8 horizon{};
    assets::Rgb8 middle{};
    assets::Rgb8 top{};

    friend bool operator==(const SkyColors&, const SkyColors&) = default;
};

/// `file` must outlive it. Without skies, nothing shows.
class RaceSky {
public:
    explicit RaceSky(const assets::SkyFile& file);

    /// Fades to the named sky and applies the show and hide flags, as an EVB sky state starts.
    void Apply(const assets::SkyStateResource& state);

    void FadeTo(std::string_view sky, Milliseconds fade);

    void Tick(Milliseconds elapsed);

    [[nodiscard]] const SkyColors& Colors() const { return colors_; }
    [[nodiscard]] float HeightOffset() const { return file_->heightOffset; }
    [[nodiscard]] bool ShowsDome() const { return !file_->skies.empty() && !isDomeHidden_; }
    [[nodiscard]] bool ShowsWorld() const { return ShowsDome() && !isWorldHidden_; }

private:
    struct Clock {
        Milliseconds elapsed{0};
        std::size_t keyframe = 0;
    };

    [[nodiscard]] std::optional<std::size_t> Find(std::string_view sky) const;
    [[nodiscard]] SkyColors ColorsOf(std::size_t sky) const;

    const assets::SkyFile* file_;
    std::vector<Clock> clocks_;
    std::size_t current_ = 0;
    std::size_t previous_ = 0;
    Milliseconds fade_{0};
    Milliseconds faded_{0};
    SkyColors colors_;
    bool isDomeHidden_ = false;
    bool isWorldHidden_ = false;
};

}
