#include "audio/voice.hpp"

#include <format>
#include <utility>

namespace rr::audio {

Result<void> Voice::Start(ma_engine& engine, ma_sound_group& bus, std::unique_ptr<PcmStream> stream,
                          VoiceStart start) {
    Stop();
    stream_ = std::move(stream);
    Result<void> source = InitStreamDataSource(source_, *stream_);
    if (!source) {
        stream_.reset();
        return source;
    }
    const ma_result result = ma_sound_init_from_data_source(
        &engine, &source_, MA_SOUND_FLAG_NO_SPATIALIZATION, &bus, &sound_);
    if (result != MA_SUCCESS) {
        UninitStreamDataSource(source_);
        stream_.reset();
        return Fail(ErrorCode::Platform,
                    std::format("can't play a sound: {}", ma_result_description(result)));
    }
    hasSound_ = true;
    Mix(start.mix);
    if (start.fadeIn > Milliseconds{0}) {
        ma_sound_set_fade_in_milliseconds(&sound_, 0.0F, 1.0F,
                                          static_cast<ma_uint64>(start.fadeIn.count()));
    }
    ma_sound_start(&sound_);
    return {};
}

void Voice::Mix(const VoiceMix& mix) {
    if (hasSound_) {
        ma_sound_set_volume(&sound_, mix.volume);
        ma_sound_set_pitch(&sound_, mix.pitch);
        ma_sound_set_pan(&sound_, mix.pan);
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

bool Voice::IsPlaying() const {
    return hasSound_ && ma_sound_is_playing(&sound_) == MA_TRUE &&
           ma_sound_at_end(&sound_) == MA_FALSE;
}

}
