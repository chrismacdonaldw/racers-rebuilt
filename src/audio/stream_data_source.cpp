#include "audio/stream_data_source.hpp"

#include <cstdint>
#include <format>
#include <span>

namespace rr::audio {
namespace {

PcmStream& StreamOf(ma_data_source* source) {
    return *static_cast<StreamDataSource*>(source)->stream;
}

ma_result OnRead(ma_data_source* source, void* framesOut, ma_uint64 frameCount,
                 ma_uint64* framesRead) {
    PcmStream& stream = StreamOf(source);
    const std::size_t sampleCount = frameCount * stream.Format().channelCount;
    const std::uint64_t read =
        stream.Read(std::span(static_cast<std::int16_t*>(framesOut), sampleCount));
    if (framesRead != nullptr) {
        *framesRead = read;
    }
    return read == 0 && frameCount > 0 ? MA_AT_END : MA_SUCCESS;
}

ma_result OnSeek(ma_data_source* source, ma_uint64 frame) {
    StreamOf(source).Seek(frame);
    return MA_SUCCESS;
}

ma_result OnGetDataFormat(ma_data_source* source, ma_format* format, ma_uint32* channelCount,
                          ma_uint32* sampleRateHz, ma_channel* channelMap,
                          std::size_t channelMapCapacity) {
    const PcmFormat streamFormat = StreamOf(source).Format();
    *format = ma_format_s16;
    *channelCount = streamFormat.channelCount;
    *sampleRateHz = streamFormat.sampleRateHz;
    ma_channel_map_init_standard(ma_standard_channel_map_default, channelMap, channelMapCapacity,
                                 streamFormat.channelCount);
    return MA_SUCCESS;
}

ma_result OnGetCursor(ma_data_source* source, ma_uint64* cursor) {
    *cursor = StreamOf(source).Cursor();
    return MA_SUCCESS;
}

ma_result OnGetLength(ma_data_source* source, ma_uint64* length) {
    *length = StreamOf(source).FrameCount();
    return MA_SUCCESS;
}

constexpr ma_data_source_vtable kVtable{
    .onRead = OnRead,
    .onSeek = OnSeek,
    .onGetDataFormat = OnGetDataFormat,
    .onGetCursor = OnGetCursor,
    .onGetLength = OnGetLength,
    .onSetLooping = nullptr,
    .flags = 0,
};

}

Result<void> InitStreamDataSource(StreamDataSource& source, PcmStream& stream) {
    source.stream = &stream;
    ma_data_source_config config = ma_data_source_config_init();
    config.vtable = &kVtable;
    const ma_result result = ma_data_source_init(&config, &source.base);
    if (result != MA_SUCCESS) {
        return Fail(ErrorCode::Platform,
                    std::format("can't create an audio source: {}", ma_result_description(result)));
    }
    return {};
}

void UninitStreamDataSource(StreamDataSource& source) {
    ma_data_source_uninit(&source.base);
}

}
