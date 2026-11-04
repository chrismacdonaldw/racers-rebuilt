#include "app/powerup/powerup_tuning_file.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <span>
#include <utility>
#include <vector>

#include "app/powerup/powerup_tuning_keys.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "settings/toml_file.hpp"
#include "sim/replay/replay_bytes.hpp"

namespace rr::app {
namespace {

using powerup_keys::IndexOf;
using powerup_keys::kColourNames;
using powerup_keys::kKindNames;
using powerup_keys::kSlotsSection;
using powerup_keys::kTurboNames;
using powerup_keys::Limits;
using powerup_keys::VisitTuning;

std::string FloatText(float value) {
    std::array<char, 32> buffer{};
    const std::to_chars_result written =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    std::string text(buffer.data(), written.ptr);
    if (text.find_first_of(".e") == std::string::npos) {
        text += ".0";
    }
    return text;
}

class TuningReader {
public:
    TuningReader(const toml::table& root, std::string_view source) : root_(root), source_(source) {}

    void Section(std::string_view name) {
        WarnUnknownKeys();
        section_ = name;
        table_ = name.empty() ? nullptr : root_[name].as_table();
        seen_.clear();
        if (!name.empty()) {
            sections_.emplace_back(name);
        }
    }

    void Number(std::string_view key, float& value, Limits limits) {
        if (const std::optional<double> read = ReadNumber(key, limits)) {
            value = static_cast<float>(*read);
        }
    }

    void Distance(std::string_view key, float& squared, Limits limits) {
        if (const std::optional<double> read = ReadNumber(key, limits)) {
            const auto distance = static_cast<float>(*read);
            squared = distance * distance;
        }
    }

    void Time(std::string_view key, Milliseconds& value, Limits limits) {
        if (const std::optional<double> read = ReadNumber(key, limits)) {
            value = Milliseconds{std::llround(*read)};
        }
    }

    template <typename T>
    void Count(std::string_view key, T& value, Limits limits) {
        if (const std::optional<double> read = ReadNumber(key, limits)) {
            value = static_cast<T>(std::lround(*read));
        }
    }

    void Numbers(std::string_view key, std::span<float> values, Limits limits) {
        ReadList(key, values.size(), [&](std::size_t i, const toml::node& node) {
            const std::optional<double> read = ClampedNumber(key, node, limits);
            if (read) {
                values[i] = static_cast<float>(*read);
            }
            return read.has_value();
        });
    }

    void Times(std::string_view key, std::span<Milliseconds> values, Limits limits) {
        ReadList(key, values.size(), [&](std::size_t i, const toml::node& node) {
            const std::optional<double> read = ClampedNumber(key, node, limits);
            if (read) {
                values[i] = Milliseconds{std::llround(*read)};
            }
            return read.has_value();
        });
    }

    void Turbos(std::string_view key, std::span<sim::TurboLevel> values) {
        ReadList(key, values.size(), [&](std::size_t i, const toml::node& node) {
            const std::optional<std::size_t> level = node.value<std::string_view>().and_then(
                [](std::string_view name) { return IndexOf(kTurboNames, name); });
            if (level) {
                values[i] = static_cast<sim::TurboLevel>(*level);
            } else {
                Warn(key, std::format("takes {}", Joined(kTurboNames)));
            }
            return level.has_value();
        });
    }

    void ReadSlots(sim::PowerupSlots& slots) {
        sections_.emplace_back(kSlotsSection);
        const toml::table* table = root_[kSlotsSection].as_table();
        if (table == nullptr) {
            return;
        }
        for (const auto& [key, node] : *table) {
            const std::optional<std::size_t> colour = IndexOf(kColourNames, key.str());
            if (!colour) {
                log::Warn("{}: [slots] has no colour {}, ignoring it", source_, key.str());
                continue;
            }
            sim::PowerupSlots::value_type read = slots.at(*colour);
            const bool isValid =
                ReadArray(key.str(), node, read.size(), [&](std::size_t i, const toml::node& kind) {
                    return ReadKind(key.str(), kind, read.at(i));
                });
            if (isValid) {
                slots.at(*colour) = read;
            }
        }
    }

    void WarnUnknownSections() const {
        for (const auto& [key, node] : root_) {
            if (!std::ranges::contains(sections_, key.str())) {
                log::Warn("{}: no power-up is called {}, ignoring it", source_, key.str());
            }
        }
    }

private:
    template <std::size_t N>
    static std::string Joined(const std::array<std::string_view, N>& names) {
        std::string joined;
        for (const std::string_view name : names) {
            joined += joined.empty() ? "" : ", ";
            joined += name;
        }
        return joined;
    }

    void Warn(std::string_view key, std::string_view problem) const {
        log::Warn("{}: [{}] {} {}, ignoring it", source_, section_, key, problem);
    }

    const toml::node* Take(std::string_view key) {
        seen_.emplace_back(key);
        return table_ == nullptr ? nullptr : table_->get(key);
    }

    [[nodiscard]] std::optional<double> ClampedNumber(std::string_view key, const toml::node& node,
                                                      Limits limits) const {
        const std::optional<double> read = node.value<double>();
        if (!read || !std::isfinite(*read)) {
            Warn(key, "isn't a number");
            return std::nullopt;
        }
        const double clamped = std::clamp(*read, limits.min, limits.max);
        if (clamped != *read) {
            log::Warn("{}: [{}] {} = {} is outside {} to {}, using {}", source_, section_, key,
                      *read, limits.min, limits.max, clamped);
        }
        return clamped;
    }

    std::optional<double> ReadNumber(std::string_view key, Limits limits) {
        const toml::node* node = Take(key);
        return node == nullptr ? std::nullopt : ClampedNumber(key, *node, limits);
    }

    template <typename ReadItem>
    [[nodiscard]] bool ReadArray(std::string_view key, const toml::node& node, std::size_t size,
                                 ReadItem readItem) const {
        const toml::array* array = node.as_array();
        if (array == nullptr || array->size() != size) {
            Warn(key, std::format("needs a list of {}", size));
            return false;
        }
        for (std::size_t i = 0; i < size; ++i) {
            if (!readItem(i, *array->get(i))) {
                return false;
            }
        }
        return true;
    }

    template <typename ReadItem>
    void ReadList(std::string_view key, std::size_t size, ReadItem readItem) {
        if (const toml::node* node = Take(key)) {
            static_cast<void>(ReadArray(key, *node, size, readItem));
        }
    }

    bool ReadKind(std::string_view key, const toml::node& node, sim::PowerupKind& kind) const {
        const std::optional<std::size_t> index = node.value<std::string_view>().and_then(
            [](std::string_view name) { return IndexOf(kKindNames, name); });
        if (!index) {
            log::Warn("{}: [slots] {} takes {}, ignoring it", source_, key, Joined(kKindNames));
            return false;
        }
        kind = static_cast<sim::PowerupKind>(*index);
        return true;
    }

    void WarnUnknownKeys() const {
        if (table_ == nullptr) {
            return;
        }
        for (const auto& [key, node] : *table_) {
            if (!std::ranges::contains(seen_, key.str())) {
                log::Warn("{}: [{}] has no key {}, ignoring it", source_, section_, key.str());
            }
        }
    }

    const toml::table& root_;
    std::string_view source_;
    std::string section_;
    const toml::table* table_ = nullptr;
    std::vector<std::string> seen_;
    std::vector<std::string> sections_;
};

class TuningWriter {
public:
    void Section(std::string_view name) {
        if (!name.empty()) {
            std::format_to(std::back_inserter(text_), "{}[{}]\n", text_.empty() ? "" : "\n", name);
        }
    }

    void Number(std::string_view key, const float& value, Limits /*limits*/) {
        Line(key, FloatText(value));
    }

    void Distance(std::string_view key, const float& squared, Limits /*limits*/) {
        Line(key, FloatText(std::sqrt(squared)));
    }

    void Time(std::string_view key, const Milliseconds& value, Limits /*limits*/) {
        Line(key, std::to_string(value.count()));
    }

    template <typename T>
    void Count(std::string_view key, const T& value, Limits /*limits*/) {
        Line(key, std::to_string(value));
    }

    void Numbers(std::string_view key, std::span<const float> values, Limits /*limits*/) {
        List(key, values, FloatText);
    }

    void Times(std::string_view key, std::span<const Milliseconds> values, Limits /*limits*/) {
        List(key, values, [](Milliseconds time) { return std::to_string(time.count()); });
    }

    void Turbos(std::string_view key, std::span<const sim::TurboLevel> values) {
        List(key, values, [](sim::TurboLevel level) {
            return std::format("\"{}\"", kTurboNames.at(static_cast<std::size_t>(level)));
        });
    }

    void WriteSlots(const sim::PowerupSlots& slots) {
        Section(kSlotsSection);
        for (std::size_t colour = 0; colour < slots.size(); ++colour) {
            List(kColourNames.at(colour), std::span<const sim::PowerupKind>(slots.at(colour)),
                 [](sim::PowerupKind kind) {
                     return std::format("\"{}\"", kKindNames.at(static_cast<std::size_t>(kind)));
                 });
        }
    }

    [[nodiscard]] std::string Text() && { return std::move(text_); }

private:
    void Line(std::string_view key, std::string_view value) {
        std::format_to(std::back_inserter(text_), "{} = {}\n", key, value);
    }

    template <typename T, typename Format>
    void List(std::string_view key, std::span<const T> values, Format format) {
        std::string joined;
        for (const T& value : values) {
            joined += joined.empty() ? "" : ", ";
            joined += format(value);
        }
        Line(key, std::format("[{}]", joined));
    }

    std::string text_;
};

}

Result<void> ApplyPowerupTuning(std::string_view text, std::string_view source,
                                sim::PowerupTuning& tuning) {
    return settings::ParseToml(text, source).transform([&](const toml::table& root) {
        TuningReader reader(root, source);
        reader.ReadSlots(tuning.slots);
        VisitTuning(reader, tuning);
        reader.WarnUnknownSections();
    });
}

sim::PowerupTuning LoadPowerupTuning(const assets::Vfs& vfs) {
    sim::PowerupTuning tuning;
    Result<std::vector<assets::FileData>> files = vfs.ReadEveryLayer(kPowerupTuningFile);
    if (!files) {
        log::Warn("{}, power-ups keep their original values", files.error().message);
        return tuning;
    }
    for (const assets::FileData& file : *files) {
        const std::span<const std::byte> bytes = file.Bytes();
        sim::PowerupTuning applied = tuning;
        const Result<void> read = ApplyPowerupTuning(
            std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
            kPowerupTuningFile, applied);
        if (read) {
            tuning = applied;
        } else {
            log::Warn("{}, skipping a mod's power-up values", read.error().message);
        }
    }
    return tuning;
}

std::string FormatPowerupTuning(const sim::PowerupTuning& tuning) {
    TuningWriter writer;
    writer.WriteSlots(tuning.slots);
    VisitTuning(writer, tuning);
    return std::move(writer).Text();
}

Result<void> DumpPowerupTuning(const std::filesystem::path& file) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << FormatPowerupTuning(sim::kOriginalPowerups);
    if (!out) {
        return Fail(ErrorCode::Io, std::format("can't write {}", Utf8FromPath(file)));
    }
    log::Info("wrote the original power-up values to {}", Utf8FromPath(file));
    return {};
}

std::optional<std::string> PowerupTuningFingerprint(const sim::PowerupTuning& tuning) {
    const std::string text = FormatPowerupTuning(tuning);
    if (text == FormatPowerupTuning(sim::kOriginalPowerups)) {
        return std::nullopt;
    }
    return std::format("powerups:{:016x}", sim::HashBytes(std::as_bytes(std::span(text))));
}

}
