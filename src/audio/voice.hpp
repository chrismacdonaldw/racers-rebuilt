#pragma once

#include <memory>

#include "audio/pcm_stream.hpp"
#include "audio/stream_data_source.hpp"
#include "core/result.hpp"

#include <miniaudio.h>

namespace rr::audio {

struct VoiceStart {
    float volume = 1.0F;
    bool isLooping = false;
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
    void Stop();

    [[nodiscard]] bool IsPlaying() const;

private:
    std::unique_ptr<PcmStream> stream_;
    StreamDataSource source_{};
    ma_sound sound_{};
    bool hasSound_ = false;
};

}
