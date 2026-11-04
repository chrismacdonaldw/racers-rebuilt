#include "assets/json_value.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <system_error>

namespace rr::assets {
namespace {

constexpr int kMaxDepth = 64;
constexpr std::uint32_t kLatinEnd = 0x100;
constexpr std::uint32_t kTwoByteEnd = 0x800;
constexpr std::uint32_t kSixBits = 0x3F;
constexpr std::uint32_t kLeadTwo = 0xC0;
constexpr std::uint32_t kLeadThree = 0xE0;
constexpr std::uint32_t kContinuation = 0x80;
constexpr int kHexBase = 16;
constexpr std::size_t kHexDigits = 4;

class JsonReader {
public:
    explicit JsonReader(std::string_view text) : text_(text) {}

    Result<JsonValue> ReadDocument() {
        Result<JsonValue> value = ReadValue(0);
        SkipSpace();
        if (value && at_ != text_.size()) {
            return Problem("there's text after the value");
        }
        return value;
    }

private:
    // NOLINTBEGIN(misc-no-recursion)
    Result<JsonValue> ReadValue(int depth) {
        SkipSpace();
        if (depth > kMaxDepth) {
            return Problem("it nests too deeply");
        }
        if (at_ >= text_.size()) {
            return Problem("it ends early");
        }
        switch (text_[at_]) {
            case '{':
                return ReadObject(depth);
            case '[':
                return ReadArray(depth);
            case '"':
                return ReadString().transform(
                    [](std::string text) { return JsonValue(JsonValue::Data(std::move(text))); });
            default:
                return ReadWord();
        }
    }

    Result<JsonValue> ReadObject(int depth) {
        ++at_;
        JsonObject object;
        if (Take('}')) {
            return JsonValue(JsonValue::Data(std::move(object)));
        }
        do {
            SkipSpace();
            Result<std::string> key = ReadString();
            if (!key) {
                return std::unexpected(std::move(key.error()));
            }
            if (!Take(':')) {
                return Problem("a key has no ':'");
            }
            Result<JsonValue> value = ReadValue(depth + 1);
            if (!value) {
                return value;
            }
            object.emplace_back(std::move(*key), std::move(*value));
        } while (Take(','));
        if (!Take('}')) {
            return Problem("an object has no '}'");
        }
        return JsonValue(JsonValue::Data(std::move(object)));
    }

    Result<JsonValue> ReadArray(int depth) {
        ++at_;
        JsonArray array;
        if (Take(']')) {
            return JsonValue(JsonValue::Data(std::move(array)));
        }
        do {
            Result<JsonValue> value = ReadValue(depth + 1);
            if (!value) {
                return value;
            }
            array.push_back(std::move(*value));
        } while (Take(','));
        if (!Take(']')) {
            return Problem("an array has no ']'");
        }
        return JsonValue(JsonValue::Data(std::move(array)));
    }
    // NOLINTEND(misc-no-recursion)

    Result<std::string> ReadString() {
        if (at_ >= text_.size() || text_[at_] != '"') {
            return Problem("a string has no '\"'");
        }
        ++at_;
        std::string text;
        while (at_ < text_.size() && text_[at_] != '"') {
            const char c = text_[at_++];
            if (c != '\\') {
                text += c;
                continue;
            }
            if (Result<void> escaped = ReadEscape(text); !escaped) {
                return std::unexpected(std::move(escaped.error()));
            }
        }
        if (at_ >= text_.size()) {
            return Problem("a string has no closing '\"'");
        }
        ++at_;
        return text;
    }

    Result<void> ReadEscape(std::string& text) {
        if (at_ >= text_.size()) {
            return Problem("a string ends in '\\'");
        }
        const char c = text_[at_++];
        constexpr std::string_view kEscapes = "\"\\/bfnrt";
        constexpr std::string_view kMeanings = "\"\\/\b\f\n\r\t";
        if (const std::size_t escape = kEscapes.find(c); escape != std::string_view::npos) {
            text += kMeanings[escape];
            return {};
        }
        if (c != 'u' || at_ + kHexDigits > text_.size()) {
            return Problem("a string has a bad escape");
        }
        std::uint32_t code = 0;
        const char* first = text_.data() + at_;
        const auto [end, error] = std::from_chars(first, first + kHexDigits, code, kHexBase);
        if (error != std::errc() || end != first + kHexDigits) {
            return Problem("a string has a bad escape");
        }
        at_ += kHexDigits;
        AppendCode(text, code);
        return {};
    }

    static void AppendCode(std::string& text, std::uint32_t code) {
        if (code < kLatinEnd) {
            text += static_cast<char>(code);
        } else if (code < kTwoByteEnd) {
            text += static_cast<char>(kLeadTwo | (code >> 6U));
            text += static_cast<char>(kContinuation | (code & kSixBits));
        } else {
            text += static_cast<char>(kLeadThree | (code >> 12U));
            text += static_cast<char>(kContinuation | ((code >> 6U) & kSixBits));
            text += static_cast<char>(kContinuation | (code & kSixBits));
        }
    }

    Result<JsonValue> ReadWord() {
        for (const auto& [word, value] :
             {std::pair{std::string_view("true"), JsonValue::Data(true)},
              std::pair{std::string_view("false"), JsonValue::Data(false)},
              std::pair{std::string_view("null"), JsonValue::Data()}}) {
            if (text_.substr(at_).starts_with(word)) {
                at_ += word.size();
                return JsonValue(value);
            }
        }
        double number = 0.0;
        const char* first = text_.data() + at_;
        const auto [end, error] = std::from_chars(first, text_.data() + text_.size(), number);
        if (error != std::errc()) {
            return Problem("a value isn't JSON");
        }
        at_ += static_cast<std::size_t>(end - first);
        return JsonValue(JsonValue::Data(number));
    }

    bool Take(char c) {
        SkipSpace();
        if (at_ < text_.size() && text_[at_] == c) {
            ++at_;
            return true;
        }
        return false;
    }

    void SkipSpace() {
        while (at_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[at_])) != 0) {
            ++at_;
        }
    }

    [[nodiscard]] std::unexpected<Error> Problem(std::string_view what) const {
        return Fail(ErrorCode::InvalidData, std::format("{} at {}", what, at_));
    }

    std::string_view text_;
    std::size_t at_ = 0;
};

}

const JsonValue* JsonValue::Find(std::string_view key) const {
    const auto* object = std::get_if<JsonObject>(&data_);
    if (object == nullptr) {
        return nullptr;
    }
    const auto found = std::ranges::find(*object, key, &JsonObject::value_type::first);
    return found == object->end() ? nullptr : &found->second;
}

std::optional<double> JsonValue::Number() const {
    const auto* number = std::get_if<double>(&data_);
    return number != nullptr ? std::optional(*number) : std::nullopt;
}

std::optional<std::string_view> JsonValue::Text() const {
    const auto* text = std::get_if<std::string>(&data_);
    return text != nullptr ? std::optional<std::string_view>(*text) : std::nullopt;
}

std::span<const JsonValue> JsonValue::Items() const {
    const auto* array = std::get_if<JsonArray>(&data_);
    return array != nullptr ? std::span<const JsonValue>(*array) : std::span<const JsonValue>();
}

Result<JsonValue> ParseJson(std::string_view text) {
    return JsonReader(text).ReadDocument();
}

}
