#include "app/loop_sidecar.hpp"

#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

#include "core/utf8_path.hpp"

#include <toml++/toml.hpp>

namespace rr::app {
namespace {

Result<std::optional<std::uint64_t>> ReadFrame(const toml::table& table, std::string_view key) {
    const toml::node* node = table.get(key);
    if (node == nullptr) {
        return std::nullopt;
    }
    const toml::value<std::int64_t>* frame = node->as_integer();
    if (frame == nullptr || frame->get() < 0) {
        return Fail(ErrorCode::InvalidData, std::format("{} must be a sample count", key));
    }
    return static_cast<std::uint64_t>(frame->get());
}

}

Result<audio::LoopPoints> ReadLoopSidecar(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return audio::LoopPoints{};
    }
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const std::string name = Utf8FromPath(path);
    toml::parse_result parsed = toml::parse(text, name);
    if (!parsed) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} line {}: {}", name, parsed.error().source().begin.line,
                                parsed.error().description()));
    }
    Result<std::optional<std::uint64_t>> start = ReadFrame(parsed.table(), "loop_start");
    Result<std::optional<std::uint64_t>> end = ReadFrame(parsed.table(), "loop_end");
    if (!start || !end) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}: {}", name, (start ? end : start).error().message));
    }
    const audio::LoopPoints loop{.startFrame = start->value_or(0), .endFrame = *end};
    if (loop.endFrame && *loop.endFrame <= loop.startFrame) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}: loop_end must come after loop_start", name));
    }
    return loop;
}

}
