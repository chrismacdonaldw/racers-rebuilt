#include "audio/voice.hpp"

#include <format>
#include <utility>

#include "audio/volume_curve.hpp"

namespace rr::audio {

Result<void> Voice::Start(ma_engine& engine, std::unique_ptr<PcmStream> stream, VoiceStart start) {
    Stop();
    stream_ = std::move(stream);
    Result<void> source = InitStreamDataSource(source_, *stream_);
    if (!source) {
        stream_.reset();
        return source;
    }
    const ma_result result = ma_sound_init_from_data_source(
        &engine, &source_, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &sound_);
    if (result != MA_SUCCESS) {
        UninitStreamDataSource(source_);
        stream_.reset();
        return Fail(ErrorCode::Platform,
                    std::format("can't play a sound: {}", ma_result_description(result)));
    }
    hasSound_ = true;
    mix_ = start.mix;
    busVolume_ = start.busVolume;
    Apply();
    if (start.fadeIn > Milliseconds{0}) {
        ma_sound_set_fade_in_milliseconds(&sound_, 0.0F, 1.0F,
                                          static_cast<ma_uint64>(start.fadeIn.count()));
    }
    ma_sound_start(&sound_);
    return {};
}

void Voice::Mix(const VoiceMix& mix) {
    mix_ = mix;
    Apply();
}

void Voice::SetBusVolume(float volume) {
    busVolume_ = volume;
    Apply();
}

void Voice::Apply() {
    if (hasSound_) {
        ma_sound_set_volume(&sound_, VolumeGain(mix_.volume * busVolume_));
        ma_sound_set_pitch(&sound_, mix_.pitch);
        ma_sound_set_pan(&sound_, PanBalance(mix_.pan));
    }
}

void Voice::Stop() {
    if (hasSound_) {
        ma_sound_uninit(&sound_);
        UninitStreamDataSource(source_);
        hasSound_ = false;
    }
    stream_.reset();
}

void Voice::FadeOut(Milliseconds fade) {
    if (hasSound_) {
        ma_sound_stop_with_fade_in_milliseconds(&sound_, static_cast<ma_uint64>(fade.count()));
    }
}

Milliseconds Voice::PlayedTime() const {
    return hasSound_
               ? Milliseconds{static_cast<std::int64_t>(ma_sound_get_time_in_milliseconds(&sound_))}
               : Milliseconds{0};
}

bool Voice::IsPlaying() const {
    return hasSound_ && ma_sound_is_playing(&sound_) == MA_TRUE &&
           ma_sound_at_end(&sound_) == MA_FALSE;
}

}
