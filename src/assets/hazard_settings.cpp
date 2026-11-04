#include "assets/hazard_settings.hpp"

#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rr::assets {
namespace {

constexpr std::size_t kVectorSize = 3;

class Settings {
public:
    Settings(const JsonValue& json, std::vector<std::string>& problems)
        : json_(&json), problems_(&problems) {}

    [[nodiscard]] float Float(std::string_view key) const {
        return Checked(json_->Find(key), key, &JsonValue::Float, 0.0F);
    }

    [[nodiscard]] EventId Event(std::string_view key) const {
        return Checked(json_->Find(key), key, &JsonValue::Integer<EventId>, kNoEvent);
    }

    [[nodiscard]] Milliseconds Duration(std::string_view key) const {
        return Milliseconds{Checked(json_->Find(key), key, &JsonValue::Integer<Milliseconds::rep>,
                                    Milliseconds::rep{0})};
    }

    [[nodiscard]] std::optional<std::string> OptionalText(std::string_view key) const {
        const JsonValue* value = json_->Find(key);
        const std::optional<std::string_view> text =
            value != nullptr ? value->Text() : std::nullopt;
        return text ? std::optional<std::string>(*text) : std::nullopt;
    }

    [[nodiscard]] std::string Text(std::string_view key) const {
        return OptionalText(key).value_or(std::string());
    }

    [[nodiscard]] std::vector<std::string> Texts(std::string_view key) const {
        std::vector<std::string> texts;
        const JsonValue* value = json_->Find(key);
        for (const JsonValue& item :
             value != nullptr ? value->Items() : std::span<const JsonValue>()) {
            texts.emplace_back(item.Text().value_or(""));
        }
        return texts;
    }

    [[nodiscard]] std::optional<Vector3> OptionalPoint(std::string_view key) const {
        const JsonValue* value = json_->Find(key);
        if (value == nullptr || value->Items().size() != kVectorSize) {
            return std::nullopt;
        }
        const std::span<const JsonValue> items = value->Items();
        const auto at = [&](std::size_t i) {
            return Checked(&items[i], key, &JsonValue::Float, 0.0F);
        };
        return Vector3{.x = at(0), .y = at(1), .z = at(2)};
    }

    [[nodiscard]] Vector3 Point(std::string_view key) const {
        return OptionalPoint(key).value_or(Vector3{});
    }

    [[nodiscard]] Settings Child(std::string_view key) const {
        static const JsonValue kNone;
        const JsonValue* value = json_->Find(key);
        return {value != nullptr ? *value : kNone, *problems_};
    }

    [[nodiscard]] Settings Item(const JsonValue& item) const { return {item, *problems_}; }

    [[nodiscard]] std::span<const JsonValue> Items(std::string_view key) const {
        return json_->Items(key);
    }

private:
    template <typename Value>
    Value Checked(const JsonValue* value, std::string_view key,
                  std::optional<Value> (JsonValue::*read)() const, Value fallback) const {
        if (value == nullptr) {
            return fallback;
        }
        const std::optional<Value> number = (value->*read)();
        if (!number) {
            problems_->push_back(std::format("{} isn't a number in range", key));
            return fallback;
        }
        return *number;
    }

    const JsonValue* json_;
    std::vector<std::string>* problems_;
};

CannonShot ShotFrom(const Settings& settings) {
    return {.launch = settings.Point("launch"),
            .target = settings.OptionalPoint("target"),
            .triggerCentre = settings.OptionalPoint("trigger_centre"),
            .triggerRadius = settings.Float("trigger_radius"),
            .event = settings.Event("event")};
}

std::vector<AimPoint> AimsFrom(const Settings& settings, std::string_view key) {
    const std::span<const JsonValue> items = settings.Items(key);
    std::vector<AimPoint> aims;
    aims.reserve(items.size());
    for (const JsonValue& item : items) {
        const Settings aim = settings.Item(item);
        aims.push_back({.position = aim.Point("position"), .event = aim.Event("event")});
    }
    return aims;
}

HazardPayload PayloadFrom(HazardKind kind, const Settings& settings) {
    switch (kind) {
        case HazardKind::Cannon:
            return ShotFrom(settings);
        case HazardKind::BreakableProp:
            return BreakableProp{.surface = settings.Text("surface"),
                                 .entities = settings.Texts("entities"),
                                 .trigger = settings.Event("trigger")};
        case HazardKind::WaterScroll:
            return WaterScroll{.entity = settings.Text("entity"),
                               .uAmplitude = settings.Float("u_amplitude"),
                               .vAmplitude = settings.Float("v_amplitude")};
        case HazardKind::ShipCannons:
            return ShipCannons{.launches = AimsFrom(settings, "launches"),
                               .targets = AimsFrom(settings, "targets"),
                               .shot = ShotFrom(settings.Child("shot"))};
        case HazardKind::SwingingBox:
            return SwingingBox{.entity = settings.Text("entity"),
                               .trigger = settings.Event("trigger"),
                               .startFrame = settings.Float("start_frame"),
                               .size = settings.Point("size")};
        case HazardKind::LightningCannon:
            return LightningCannon{.trigger = settings.Event("trigger"),
                                   .entity = settings.OptionalText("entity"),
                                   .source = settings.OptionalPoint("source"),
                                   .period = settings.Duration("period_ms"),
                                   .sweepRadians = settings.Float("sweep_radians"),
                                   .verticalRange = settings.Float("vertical_range"),
                                   .verticalBase = settings.Float("vertical_base")};
        case HazardKind::DragonCannonballs:
            return DragonCannonballs{.material = settings.OptionalText("material"),
                                     .entity = settings.Text("entity"),
                                     .trigger = settings.Event("trigger"),
                                     .interval = settings.Duration("interval_ms")};
        case HazardKind::UfoGrabber:
            return UfoGrabber{.trigger = settings.Event("trigger"),
                              .entity = settings.Text("entity"),
                              .pullStrength = settings.Float("pull_strength"),
                              .grabFrameLow = settings.Float("grab_frame_low"),
                              .grabFrameHigh = settings.Float("grab_frame_high")};
        default:
            return std::monostate{};
    }
}

}

Result<HazardPayload> HazardPayloadFrom(HazardKind kind, const JsonValue& settings) {
    std::vector<std::string> problems;
    HazardPayload payload = PayloadFrom(kind, Settings(settings, problems));
    if (!problems.empty()) {
        return Fail(ErrorCode::InvalidData, problems.front());
    }
    return payload;
}

}
