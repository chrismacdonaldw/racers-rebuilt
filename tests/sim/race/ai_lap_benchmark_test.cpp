#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/game_path.hpp"
#include "assets/race_catalog.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/track_loader.hpp"
#include "sim/ai/ai_skill.hpp"
#include "sim/ai/powerup_habits.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/physics/racer_chassis.hpp"
#include "sim/powerup/every_effect.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/race_track.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr int kMaxTicks = 100000;
constexpr Milliseconds kStragglerWait{60000};
constexpr std::uint64_t kSeed = 1;

struct Mode {
    std::string_view name;
    AiDriving driving = AiDriving::Route;
    AiCraft craft;
    std::optional<AiSkill> skill;
};

constexpr AiCraft kCraft{.brakes = true, .passes = true, .unsticks = true, .aims = true};

std::array<Mode, 8> Modes() {
    const AiSkill equalMaster{.spread = SkillSpread::Equal, .difficulty = kMasterLevel};
    return {
        Mode{.name = "route"},
        Mode{.name = "physics", .driving = AiDriving::Physics},
        Mode{.name = "physics+craft", .driving = AiDriving::Physics, .craft = kCraft},
        Mode{
            .name = "skill o3", .driving = AiDriving::Physics, .craft = kCraft, .skill = AiSkill{}},
        Mode{.name = "skill e1",
             .driving = AiDriving::Physics,
             .craft = kCraft,
             .skill = AiSkill{.spread = SkillSpread::Equal, .difficulty = kEasiestLevel}},
        Mode{.name = "skill e5",
             .driving = AiDriving::Physics,
             .craft = kCraft,
             .skill = equalMaster},
        Mode{.name = "physics+line", .driving = AiDriving::Physics, .craft = AiCraft::All()},
        Mode{.name = "skill e5+line",
             .driving = AiDriving::Physics,
             .craft = AiCraft::All(),
             .skill = equalMaster}};
}

std::string HomeCircuit(const assets::Vfs& vfs, const std::string& track) {
    const Result<assets::RaceCatalog> races = assets::RaceCatalog::Load(vfs);
    const std::optional<std::size_t> home = races ? races->HomeCircuit(track) : std::nullopt;
    return home ? races->Circuits()[*home].name : std::string{};
}

Result<RaceSetup> ComputerGrid(const assets::Vfs& vfs, const std::string& track,
                               std::size_t slotCount, const Mode& mode) {
    const std::string circuit = HomeCircuit(vfs, track);
    Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(vfs);
    Result<std::vector<std::string>> codes = catalog.and_then(
        [&](const assets::RacerCatalog&) { return assets::LoadGrid(vfs, track, circuit); });
    if (!codes) {
        return std::unexpected(codes.error());
    }
    RaceSetup setup{.track = track,
                    .circuit = circuit,
                    .seed = kSeed,
                    .aiDriving = mode.driving,
                    .aiCraft = mode.craft,
                    .aiSkill = mode.skill,
                    .effects = EveryEffect()};
    for (std::size_t i = 0; i < slotCount && i < codes->size(); ++i) {
        Result<assets::RacerFiles> files = catalog->Find(codes->at(i));
        if (!files) {
            return std::unexpected(files.error());
        }
        setup.racers.push_back({.chassis = PrebuiltChassis(files->champion, files->chassis),
                                .habits = HabitsOf(files->driver)});
    }
    return setup;
}

std::optional<float> MeanLapSeconds(const RacerProgress& progress) {
    if (!progress.finishPlace || progress.lapTimes.empty()) {
        return std::nullopt;
    }
    const Milliseconds total =
        std::accumulate(progress.lapTimes.begin(), progress.lapTimes.end(), Milliseconds{0});
    return static_cast<float>(total.count()) / 1000.0F /
           static_cast<float>(progress.lapTimes.size());
}

bool HasAnyFinished(const RaceSession& session) {
    for (std::size_t i = 0; i < session.Racers().size(); ++i) {
        if (session.Progress().OfRacer(i).finishPlace) {
            return true;
        }
    }
    return false;
}

bool HaveAllFinished(const RaceSession& session) {
    for (std::size_t i = 0; i < session.Racers().size(); ++i) {
        if (!session.Progress().OfRacer(i).finishPlace) {
            return false;
        }
    }
    return true;
}

std::vector<std::optional<float>> RaceLaps(const assets::Vfs& vfs, const std::string& track,
                                           const Mode& mode) {
    Result<RaceTrack> loaded = LoadRaceTrack(vfs, track, false);
    REQUIRE(loaded.has_value());
    Result<RaceSetup> setup = ComputerGrid(vfs, track, loaded->grid.size(), mode);
    REQUIRE(setup.has_value());
    const WorldList world(loaded->collision.Track());
    Result<RaceSession> session = RaceSession::Create(*setup, CourseOf(*loaded, world));
    REQUIRE(session.has_value());
    const std::vector<RacerControls> idle(session->Racers().size());
    Milliseconds sinceFirstFinish{0};
    for (int ticks = 0;
         ticks < kMaxTicks && sinceFirstFinish < kStragglerWait && !HaveAllFinished(*session);
         ++ticks) {
        session->Tick(kTick, idle);
        sinceFirstFinish += HasAnyFinished(*session) ? kTick : Milliseconds{0};
    }
    std::vector<std::optional<float>> laps;
    laps.reserve(session->Racers().size());
    for (std::size_t i = 0; i < session->Racers().size(); ++i) {
        laps.push_back(MeanLapSeconds(session->Progress().OfRacer(i)));
    }
    return laps;
}

std::string LapText(std::optional<float> seconds) {
    return seconds ? std::format("{:6.2f}", *seconds) : std::string("   DNF");
}

class SlotMeans {
public:
    void Add(const std::vector<std::optional<float>>& laps) {
        totals_.resize(laps.size());
        counts_.resize(laps.size());
        for (std::size_t i = 0; i < laps.size(); ++i) {
            totals_[i] += laps[i].value_or(0.0F);
            counts_[i] += laps[i] ? 1 : 0;
        }
    }

    [[nodiscard]] std::vector<std::optional<float>> Means() const {
        std::vector<std::optional<float>> means;
        means.reserve(totals_.size());
        for (std::size_t i = 0; i < totals_.size(); ++i) {
            means.push_back(counts_[i] == 0
                                ? std::nullopt
                                : std::optional(totals_[i] / static_cast<float>(counts_[i])));
        }
        return means;
    }

private:
    std::vector<float> totals_;
    std::vector<int> counts_;
};

std::string LapsLine(std::string_view label, std::string_view mode,
                     const std::vector<std::optional<float>>& laps) {
    std::string line = std::format("{} {:>14}:", label, mode);
    for (const std::optional<float>& lap : laps) {
        line += " " + LapText(lap);
    }
    return line;
}

TEST_CASE("benchmark: mean lap seconds per grid slot on every retail track" * doctest::skip()) {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const Mode& mode : Modes()) {
        SlotMeans means;
        for (const std::string& track : assets::ListRaceTracks(*vfs)) {
            const std::vector<std::optional<float>> laps = RaceLaps(*vfs, track, mode);
            means.Add(laps);
            MESSAGE(LapsLine(track, mode.name, laps));
        }
        MESSAGE(LapsLine("mean", mode.name, means.Means()));
    }
}

}
}
