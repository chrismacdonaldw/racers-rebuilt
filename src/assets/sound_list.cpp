#include "assets/sound_list.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

namespace rr::assets {
namespace {

constexpr std::size_t kMaxSounds = 1000;

class LineReader {
public:
    explicit LineReader(std::string_view text) : text_(text) {}

    std::optional<std::string_view> Next() {
        if (text_.empty()) {
            return std::nullopt;
        }
        const std::size_t end = std::min(text_.find('\n'), text_.size());
        std::string_view line = text_.substr(0, end);
        text_.remove_prefix(std::min(end + 1, text_.size()));
        if (line.ends_with('\r')) {
            line.remove_suffix(1);
        }
        return line;
    }

private:
    std::string_view text_;
};

std::optional<std::size_t> ParseCount(std::string_view text) {
    std::size_t count = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), count);
    if (error != std::errc() || end != text.data() + text.size() || count > kMaxSounds) {
        return std::nullopt;
    }
    return count;
}

}

Result<SoundList> ParseSoundList(std::span<const std::byte> bytes) {
    LineReader lines(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    const std::optional<std::string_view> folder = lines.Next();
    const std::optional<std::string_view> countLine = lines.Next();
    const std::optional<std::size_t> count = countLine.and_then(ParseCount);
    if (!folder || !count) {
        return Fail(ErrorCode::InvalidData, "a sound list starts with a folder and a count");
    }
    SoundList list{.folder = std::string(*folder)};
    std::ranges::replace(list.folder, '\\', '/');
    list.names.reserve(*count);
    while (list.names.size() < *count) {
        const std::optional<std::string_view> name = lines.Next();
        if (!name || name->empty()) {
            return Fail(
                ErrorCode::InvalidData,
                std::format("a sound list names {} of its {} sounds", list.names.size(), *count));
        }
        list.names.emplace_back(*name);
    }
    return list;
}

}
