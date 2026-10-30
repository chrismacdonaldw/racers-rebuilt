#pragma once

#include <memory>

#include "audio/pcm_stream.hpp"
#include "audio/stream_data_source.hpp"
#include "audio/voice_mix.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"

#include <miniaudio.h>

namespace rr::audio {

struct VoiceStart {
    VoiceMix mix;
    Milliseconds fadeIn{0};
};

class Voice {
public:
    Voice() = default;
    Voice(const Voice&) = delete;
    Voice& operator=(const Voice&) = delete;
    Voice(Voice&&) = delete;
    Voice& operator=(Voice&&) = delete;
    ~Voice() { Stop(); }

    [[nodiscard]] Result<void> Start(ma_engine& engine, ma_sound_group& bus,
                                     std::unique_ptr<PcmStream> stream, VoiceStart start);
    void Mix(const VoiceMix& mix);
    void Stop();
    void FadeOut(Milliseconds fade);

    [[nodiscard]] bool IsPlaying() const;

private:
    std::unique_ptr<PcmStream> stream_;
    StreamDataSource source_{};
    ma_sound sound_{};
    bool hasSound_ = false;
};

}
