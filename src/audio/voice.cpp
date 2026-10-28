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
        &engine, &source_, MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH, &bus, &sound_);
    if (result != MA_SUCCESS) {
        UninitStreamDataSource(source_);
        stream_.reset();
        return Fail(ErrorCode::Platform,
                    std::format("can't play a sound: {}", ma_result_description(result)));
    }
    hasSound_ = true;
    ma_sound_set_volume(&sound_, start.volume);
    ma_sound_set_looping(&sound_, start.isLooping ? MA_TRUE : MA_FALSE);
    ma_sound_start(&sound_);
    return {};
}

void Voice::Stop() {
    if (hasSound_) {
        ma_sound_uninit(&sound_);
        UninitStreamDataSource(source_);
        hasSound_ = false;
    }
    stream_.reset();
}

bool Voice::IsPlaying() const {
    return hasSound_ && ma_sound_is_playing(&sound_) == MA_TRUE &&
           ma_sound_at_end(&sound_) == MA_FALSE;
}

}
