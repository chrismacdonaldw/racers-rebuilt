#include "app/ambient_sounds.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace rr::app {
namespace {

constexpr std::int32_t kFirstAmbientId = 3000;
constexpr std::uint32_t kRandomTableSize = 1024;
constexpr Milliseconds kShortestWait{500};
constexpr std::uint32_t kWaitSpreadMs = 1000;

std::optional<Vector3> SourceOf(const assets::SoundResource& record, const sim::TrackEvent& event) {
    if (record.trigger.usesEventPosition && event.position) {
        return event.position;
    }
    return record.position ? record.position : event.position;
}

audio::VoiceMix MixOf(const assets::SoundResource& record) {
    return {.volume = record.volume, .pitch = record.frequencyScale};
}

SoundRange RangeOf(const assets::SoundResource& record) {
    return {.min = record.minDistance, .max = record.maxDistance};
}

}

std::optional<audio::SoundId> AmbientSoundAt(std::span<const audio::SoundId> bank,
                                             std::int32_t soundId) {
    const std::int32_t index = soundId - kFirstAmbientId;
    if (index < 0 || std::cmp_greater_equal(index, bank.size())) {
        return std::nullopt;
    }
    return bank[static_cast<std::size_t>(index)];
}

AmbientSounds::AmbientSounds(std::span<const assets::SoundResource> records,
                             std::span<const audio::SoundId> bank, sim::Random random)
    : random_(random) {
    ambients_.reserve(records.size());
    for (const assets::SoundResource& record : records) {
        ambients_.push_back(
            {.record = record, .sound = AmbientSoundAt(bank, record.soundId), .wait = DrawWait()});
    }
}

void AmbientSounds::Hear(std::span<const sim::TrackEvent> events, const Listener& listener,
                         Milliseconds elapsed, audio::Mixer& mixer) {
    listener_ = listener;
    for (const sim::TrackEvent& event : events) {
        if (event.racer) {
            continue;
        }
        for (Ambient& ambient : ambients_) {
            if (ambient.record.trigger.event == event.id) {
                Apply(ambient, event, mixer);
            }
        }
    }
    for (Ambient& ambient : ambients_) {
        Update(ambient, elapsed, mixer);
    }
}

void AmbientSounds::Silence(audio::Mixer& mixer) {
    for (Ambient& ambient : ambients_) {
        ambient.loop.Stop(mixer);
    }
}

std::size_t AmbientSounds::RunningCount() const {
    return static_cast<std::size_t>(
        std::ranges::count_if(ambients_, [](const Ambient& ambient) { return ambient.isRunning; }));
}

void AmbientSounds::Apply(Ambient& ambient, const sim::TrackEvent& event, audio::Mixer& mixer) {
    const assets::EventTrigger& trigger = ambient.record.trigger;
    if (event.edge == sim::EventEdge::Start) {
        if (!trigger.startsOnEnd) {
            Begin(ambient, event, mixer);
        }
        return;
    }
    if (trigger.startsOnEnd) {
        Begin(ambient, event, mixer);
    }
    if (!trigger.ignoresEnd) {
        Finish(ambient, mixer);
    }
}

void AmbientSounds::Begin(Ambient& ambient, const sim::TrackEvent& event, audio::Mixer& mixer) {
    const std::optional<Vector3> source = SourceOf(ambient.record, event);
    if (ambient.isRunning || !source || !ambient.sound) {
        return;
    }
    ambient.position = *source;
    if (ambient.record.isLooping || ambient.record.probability < 1.0F) {
        ambient.isRunning = true;
        return;
    }
    PlayOnce(ambient, mixer);
}

void AmbientSounds::Finish(Ambient& ambient, audio::Mixer& mixer) {
    ambient.isRunning = false;
    ambient.loop.Stop(mixer);
}

void AmbientSounds::Update(Ambient& ambient, Milliseconds elapsed, audio::Mixer& mixer) {
    if (!ambient.isRunning) {
        return;
    }
    if (ambient.record.isLooping) {
        ambient.loop.Play(mixer, *ambient.sound,
                          PlaceSound(listener_, ambient.position, MixOf(ambient.record),
                                     RangeOf(ambient.record)));
        return;
    }
    ambient.wait -= elapsed;
    if (ambient.wait > Milliseconds{0}) {
        return;
    }
    const auto chance = static_cast<float>(random_.Below(kRandomTableSize));
    if (chance < ambient.record.probability * static_cast<float>(kRandomTableSize)) {
        PlayOnce(ambient, mixer);
    }
    ambient.wait = DrawWait();
}

void AmbientSounds::PlayOnce(const Ambient& ambient, audio::Mixer& mixer) const {
    const std::array cue{
        audio::SoundCue{.sound = *ambient.sound,
                        .mix = PlaceSound(listener_, ambient.position, MixOf(ambient.record),
                                          RangeOf(ambient.record))}};
    mixer.Play(cue);
}

Milliseconds AmbientSounds::DrawWait() {
    return kShortestWait + Milliseconds{random_.Below(kWaitSpreadMs)};
}

}
