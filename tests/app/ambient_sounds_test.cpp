#include "app/ambient_sounds.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <string_view>
#include <vector>

#include "../audio/alp_builder.hpp"
#include "assets/archive_survey.hpp"

namespace rr::app {
namespace {

constexpr std::uint32_t kRateHz = 22050;
constexpr std::size_t kFramesPerTick = 265;
constexpr Milliseconds kTick{12};
constexpr assets::EventId kEvent = 5;
constexpr std::array<std::uint8_t, 256> kPayload = [] {
    std::array<std::uint8_t, 256> payload{};
    payload.fill(0x77);
    return payload;
}();

std::span<const std::byte> Alp() {
    static const std::vector<std::byte> alp = test::BuildAlp(1, kRateHz, kPayload);
    return alp;
}

audio::Mixer HeadlessMixer() {
    Result<audio::Mixer> mixer = audio::Mixer::CreateHeadless(kRateHz);
    REQUIRE(mixer.has_value());
    return std::move(mixer).value();
}

sim::TrackEvent At(sim::EventEdge edge) {
    return {.id = kEvent, .edge = edge, .position = Vector3{}};
}

assets::SoundResource Record() {
    return {.trigger = {.event = kEvent}, .soundId = 3000};
}

class Ambience {
public:
    explicit Ambience(const assets::SoundResource& record)
        : mixer_(HeadlessMixer()),
          sound_(mixer_.AddSound(assets::FileData(Alp())).value_or(audio::SoundId{})),
          records_{record},
          ambience_(records_, std::span(&sound_, 1), sim::Random(1)) {}

    Ambience(const Ambience&) = delete;
    Ambience& operator=(const Ambience&) = delete;
    Ambience(Ambience&&) = delete;
    Ambience& operator=(Ambience&&) = delete;
    ~Ambience() { ambience_.Silence(mixer_); }

    bool Hear(std::initializer_list<sim::TrackEvent> events) {
        ambience_.Hear(std::span(events.begin(), events.size()), {}, kTick, mixer_);
        mixer_.Render(output_);
        return std::ranges::any_of(output_, [](float sample) { return sample != 0.0F; });
    }

    [[nodiscard]] std::size_t RunningCount() const { return ambience_.RunningCount(); }

private:
    audio::Mixer mixer_;
    audio::SoundId sound_;
    std::array<assets::SoundResource, 1> records_;
    AmbientSounds ambience_;
    std::array<float, kFramesPerTick * 2> output_{};
};

TEST_CASE("a looping ambient sound plays from its event's start until its end") {
    assets::SoundResource record = Record();
    record.isLooping = true;
    Ambience ambience(record);

    CHECK_FALSE(ambience.Hear({}));
    CHECK(ambience.Hear({At(sim::EventEdge::Start)}));
    CHECK(ambience.Hear({}));
    CHECK(ambience.RunningCount() == 1);
    CHECK_FALSE(ambience.Hear({At(sim::EventEdge::End)}));
    CHECK(ambience.RunningCount() == 0);
}

TEST_CASE("a one-shot ambient sound plays on each start and never runs") {
    Ambience ambience(Record());

    CHECK(ambience.Hear({At(sim::EventEdge::Start)}));
    CHECK(ambience.RunningCount() == 0);
}

TEST_CASE("an ambient sound with a probability rolls while its event runs") {
    assets::SoundResource record = Record();
    record.probability = 0.0F;
    Ambience ambience(record);

    CHECK_FALSE(ambience.Hear({At(sim::EventEdge::Start)}));
    CHECK(ambience.RunningCount() == 1);
    CHECK_FALSE(ambience.Hear({At(sim::EventEdge::End)}));
    CHECK(ambience.RunningCount() == 0);
}

TEST_CASE("an ambient sound that triggers on end plays when its event ends") {
    assets::SoundResource record = Record();
    record.trigger.startsOnEnd = true;
    Ambience ambience(record);

    CHECK_FALSE(ambience.Hear({At(sim::EventEdge::Start)}));
    CHECK(ambience.Hear({At(sim::EventEdge::End)}));
}

TEST_CASE("a looping ambient sound with no end keeps playing past its event") {
    assets::SoundResource record = Record();
    record.isLooping = true;
    record.trigger.ignoresEnd = true;
    Ambience ambience(record);

    CHECK(ambience.Hear({At(sim::EventEdge::Start)}));
    CHECK(ambience.Hear({At(sim::EventEdge::End)}));
    CHECK(ambience.RunningCount() == 1);
}

TEST_CASE("ambient sounds ignore racer events and sounds outside the bank") {
    Ambience ambience(Record());
    assets::SoundResource missing = Record();
    missing.soundId = 3001;
    Ambience outside(missing);
    const sim::TrackEvent racer{.id = kEvent, .edge = sim::EventEdge::Start, .racer = 0};

    CHECK_FALSE(ambience.Hear({racer}));
    CHECK_FALSE(outside.Hear({At(sim::EventEdge::Start)}));
}

TEST_CASE("an ambient sound with no position to play at stays silent") {
    Ambience ambience(Record());

    CHECK_FALSE(ambience.Hear({{.id = kEvent, .edge = sim::EventEdge::Start}}));
}

std::vector<assets::SoundResource> RetailSounds(std::string_view track) {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return {};
    }
    Result<assets::EventTableAsset> table = vfs->Read(std::format("GAMEDATA/{}/EVENT.EVB", track))
                                                .and_then([](const assets::FileData& data) {
                                                    return assets::ParseEventTable(data.Bytes());
                                                });
    REQUIRE(table.has_value());
    return std::move(table->sounds);
}

TEST_CASE("RACEC0R0's environment events run the ambient sounds its spec lists") {
    const std::vector<assets::SoundResource> records = RetailSounds("RACEC0R0");
    if (records.empty()) {
        return;
    }
    audio::Mixer mixer = HeadlessMixer();
    const std::vector<audio::SoundId> bank(
        20, mixer.AddSound(assets::FileData(Alp())).value_or(audio::SoundId{}));
    AmbientSounds ambience(records, bank, sim::Random(1));
    const auto fire = [&](assets::EventId id, sim::EventEdge edge) {
        const std::array events{sim::TrackEvent{.id = id, .edge = edge, .position = Vector3{}}};
        ambience.Hear(events, {}, kTick, mixer);
        return ambience.RunningCount();
    };

    const std::vector<std::size_t> running{
        fire(11, sim::EventEdge::Start), fire(2, sim::EventEdge::Start),
        fire(11, sim::EventEdge::End), fire(2, sim::EventEdge::End)};
    ambience.Silence(mixer);

    CHECK(running == std::vector<std::size_t>{3, 4, 1, 0});
}

TEST_CASE("ambient sound ids count from 3000") {
    const std::array bank{audio::SoundId{.index = 7}, audio::SoundId{.index = 9}};

    CHECK(AmbientSoundAt(bank, 3001).value_or(audio::SoundId{}).index == 9);
    CHECK_FALSE(AmbientSoundAt(bank, 2999).has_value());
    CHECK_FALSE(AmbientSoundAt(bank, 3002).has_value());
}

}
}
