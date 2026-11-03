#include "assets/avi_file.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <utility>

#include "assets/byte_reader.hpp"

namespace rr::assets {
namespace {

constexpr std::uint32_t kRiff = FourCc("RIFF");
constexpr std::uint32_t kList = FourCc("LIST");
constexpr std::uint16_t kPcmFormat = 1;
constexpr std::uint16_t kPcmBits = 16;
constexpr std::size_t kChunkHeaderSize = 8;
constexpr std::size_t kStreamHeaderRates = 20;
constexpr std::size_t kBitmapHeaderSize = 4;
constexpr std::int64_t kMicrosecondsPerSecond = 1'000'000;
constexpr std::uint32_t kMaxVideoSize = 4096;
constexpr std::uint32_t kMaxFramesPerSecond = 1000;

struct Chunk {
    std::uint32_t id = 0;
    std::uint32_t listType = 0;
    std::span<const std::byte> data;
};

struct Stream {
    std::uint32_t type = 0;
    AviFrameRate rate;
    std::span<const std::byte> format;
};

struct Lists {
    std::span<const std::byte> header;
    std::span<const std::byte> movie;
};

Result<std::vector<Chunk>> ReadChunks(std::span<const std::byte> bytes) {
    std::vector<Chunk> chunks;
    std::size_t position = 0;
    while (position + kChunkHeaderSize <= bytes.size()) {
        ByteReader reader(bytes, position);
        const std::uint32_t id = *reader.ReadU32();
        const std::uint32_t size = *reader.ReadU32();
        const Result<std::span<const std::byte>> data = reader.ReadBytes(size);
        if (!data) {
            return Fail(ErrorCode::InvalidData,
                        std::format("AVI chunk at {} runs past the end", position));
        }
        Chunk chunk{.id = id, .data = *data};
        if ((id == kList || id == kRiff) && chunk.data.size() >= 4) {
            ByteReader type(chunk.data, 0);
            chunk.listType = *type.ReadU32();
            chunk.data = chunk.data.subspan(4);
        }
        chunks.push_back(chunk);
        position = reader.Position() + (size % 2);
    }
    return chunks;
}

std::span<const std::byte> FindChunk(std::span<const Chunk> chunks, std::uint32_t id) {
    const auto found = std::ranges::find(chunks, id, &Chunk::id);
    return found == chunks.end() ? std::span<const std::byte>() : found->data;
}

Result<std::vector<Stream>> ReadStreams(std::span<const Chunk> header) {
    std::vector<Stream> streams;
    for (const Chunk& list : header) {
        if (list.id != kList || list.listType != FourCc("strl")) {
            continue;
        }
        Result<std::vector<Chunk>> chunks = ReadChunks(list.data);
        if (!chunks) {
            return std::unexpected(std::move(chunks.error()));
        }
        ByteReader strh(FindChunk(*chunks, FourCc("strh")), 0);
        const Result<std::uint32_t> type = strh.ReadU32();
        const Result<std::span<const std::byte>> skipped = strh.ReadBytes(kStreamHeaderRates - 4);
        const Result<std::uint32_t> scale = strh.ReadU32();
        const Result<std::uint32_t> rate = strh.ReadU32();
        if (!type || !skipped || !scale || !rate) {
            return Fail(ErrorCode::InvalidData, "AVI stream header is too short");
        }
        streams.push_back({.type = *type,
                           .rate = {.frames = *rate, .seconds = *scale},
                           .format = FindChunk(*chunks, FourCc("strf"))});
    }
    return streams;
}

Result<AviVideo> ReadVideoFormat(const Stream& stream) {
    ByteReader reader(stream.format, kBitmapHeaderSize);
    const Result<std::uint32_t> width = reader.ReadU32();
    const Result<std::uint32_t> height = reader.ReadU32();
    const Result<std::span<const std::byte>> planesAndBits = reader.ReadBytes(4);
    const Result<std::uint32_t> codec = reader.ReadU32();
    if (!width || !height || !planesAndBits || !codec) {
        return Fail(ErrorCode::InvalidData, "AVI video format is too short");
    }
    if (*width == 0 || *height == 0 || *width > kMaxVideoSize || *height > kMaxVideoSize) {
        return Fail(ErrorCode::InvalidData,
                    std::format("AVI video is {}x{} pixels", *width, *height));
    }
    if (stream.rate.frames == 0 || stream.rate.seconds == 0 ||
        stream.rate.frames / stream.rate.seconds > kMaxFramesPerSecond) {
        return Fail(ErrorCode::InvalidData, std::format("AVI video runs at {}/{} frames a second",
                                                        stream.rate.frames, stream.rate.seconds));
    }
    return AviVideo{.codec = *codec, .width = *width, .height = *height, .rate = stream.rate};
}

std::optional<AviAudio> ReadAudioFormat(const Stream& stream) {
    ByteReader reader(stream.format, 0);
    const Result<std::uint16_t> format = reader.ReadU16();
    const Result<std::uint16_t> channels = reader.ReadU16();
    const Result<std::uint32_t> sampleRate = reader.ReadU32();
    const Result<std::span<const std::byte>> rates = reader.ReadBytes(6);
    const Result<std::uint16_t> bits = reader.ReadU16();
    if (!format || !channels || !sampleRate || !rates || !bits || *format != kPcmFormat ||
        *bits != kPcmBits || *channels == 0) {
        return std::nullopt;
    }
    return AviAudio{.channels = *channels, .sampleRate = *sampleRate};
}

std::size_t StreamOf(std::uint32_t id) {
    const auto digit = [](std::uint32_t value) {
        return static_cast<std::size_t>((value & 0xFFU) - '0');
    };
    return (digit(id) * 10) + digit(id >> 8U);
}

bool IsVideoFrame(std::uint32_t id) {
    const std::uint32_t kind = id >> 16U;
    return kind == (FourCc("00dc") >> 16U) || kind == (FourCc("00db") >> 16U);
}

void AppendSamples(std::span<const std::byte> data, std::vector<std::int16_t>& samples) {
    const std::size_t start = samples.size();
    samples.resize(start + (data.size() / sizeof(std::int16_t)));
    std::memcpy(samples.data() + start, data.data(),
                (samples.size() - start) * sizeof(std::int16_t));
}

Result<void> ReadMovie(std::span<const std::byte> movie, std::size_t videoStream,
                       std::optional<std::size_t> audioStream, AviFile& file) {
    std::vector<Chunk> pending{{.id = kList, .data = movie}};
    while (!pending.empty()) {
        const Chunk chunk = pending.back();
        pending.pop_back();
        if (chunk.id == kList) {
            Result<std::vector<Chunk>> children = ReadChunks(chunk.data);
            if (!children) {
                return std::unexpected(std::move(children.error()));
            }
            pending.insert(pending.end(), children->rbegin(), children->rend());
            continue;
        }
        const std::size_t stream = StreamOf(chunk.id);
        if (stream == videoStream && IsVideoFrame(chunk.id)) {
            file.video.frames.push_back(chunk.data);
        } else if (file.audio && stream == audioStream) {
            AppendSamples(chunk.data, file.audio->samples);
        }
    }
    return {};
}

Result<Lists> ReadLists(std::span<const std::byte> bytes) {
    ByteReader start(bytes, 4);
    const Result<std::uint32_t> riffSize = start.ReadU32();
    const std::size_t riffEnd =
        riffSize ? std::min<std::size_t>(bytes.size(), *riffSize + kChunkHeaderSize) : 0;
    Result<std::vector<Chunk>> riff = ReadChunks(bytes.first(riffEnd));
    if (!riff || riff->empty() || riff->front().id != kRiff ||
        riff->front().listType != FourCc("AVI ")) {
        return Fail(ErrorCode::InvalidData, "not an AVI file");
    }
    Result<std::vector<Chunk>> top = ReadChunks(riff->front().data);
    if (!top) {
        return std::unexpected(std::move(top.error()));
    }
    Lists lists;
    for (const Chunk& chunk : *top) {
        if (chunk.id == kList && chunk.listType == FourCc("hdrl")) {
            lists.header = chunk.data;
        } else if (chunk.id == kList && chunk.listType == FourCc("movi")) {
            lists.movie = chunk.data;
        }
    }
    return lists;
}

struct StreamPick {
    std::size_t video = 0;
    std::optional<std::size_t> audio;
};

Result<StreamPick> PickStreams(std::span<const Stream> streams, AviFile& file) {
    std::optional<std::size_t> video;
    std::optional<std::size_t> audio;
    for (std::size_t i = 0; i < streams.size(); ++i) {
        const Stream& stream = streams[i];
        if (!video && stream.type == FourCc("vids")) {
            Result<AviVideo> format = ReadVideoFormat(stream);
            if (!format) {
                return std::unexpected(std::move(format.error()));
            }
            file.video = std::move(*format);
            video = i;
        } else if (!audio && stream.type == FourCc("auds")) {
            file.audio = ReadAudioFormat(stream);
            audio = file.audio ? std::optional(i) : std::nullopt;
        }
    }
    if (!video) {
        return Fail(ErrorCode::InvalidData, "AVI file has no video stream");
    }
    return StreamPick{.video = *video, .audio = audio};
}

}

std::size_t FrameAt(const AviVideo& video, Microseconds elapsed) {
    return static_cast<std::size_t>(elapsed.count() * video.rate.frames /
                                    (kMicrosecondsPerSecond * video.rate.seconds));
}

Result<AviFile> ParseAvi(std::span<const std::byte> bytes) {
    const Result<Lists> lists = ReadLists(bytes);
    if (!lists) {
        return std::unexpected(lists.error());
    }
    Result<std::vector<Chunk>> header = ReadChunks(lists->header);
    Result<std::vector<Stream>> streams =
        header ? ReadStreams(*header) : std::unexpected(std::move(header.error()));
    if (!streams) {
        return std::unexpected(std::move(streams.error()));
    }
    AviFile file;
    const Result<StreamPick> pick = PickStreams(*streams, file);
    if (!pick) {
        return std::unexpected(pick.error());
    }
    Result<void> read = ReadMovie(lists->movie, pick->video, pick->audio, file);
    if (!read) {
        return std::unexpected(std::move(read.error()));
    }
    return file;
}

}
