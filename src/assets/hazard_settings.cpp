#include "assets/hazard_settings.hpp"

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
    explicit Settings(const JsonValue& json) : json_(&json) {}

    [[nodiscard]] float Float(std::string_view key) const {
        const JsonValue* value = json_->Find(key);
        return value != nullptr ? static_cast<float>(value->Number().value_or(0.0)) : 0.0F;
    }

    [[nodiscard]] EventId Event(std::string_view key) const {
        const JsonValue* value = json_->Find(key);
        return value != nullptr ? static_cast<EventId>(value->Number().value_or(kNoEvent))
                                : kNoEvent;
    }

    [[nodiscard]] Milliseconds Duration(std::string_view key) const {
        return Milliseconds{static_cast<Milliseconds::rep>(Float(key))};
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
        return Vector3{.x = static_cast<float>(items[0].Number().value_or(0.0)),
                       .y = static_cast<float>(items[1].Number().value_or(0.0)),
                       .z = static_cast<float>(items[2].Number().value_or(0.0))};
    }

    [[nodiscard]] Vector3 Point(std::string_view key) const {
        return OptionalPoint(key).value_or(Vector3{});
    }

    [[nodiscard]] Settings Child(std::string_view key) const {
        static const JsonValue kNone;
        const JsonValue* value = json_->Find(key);
        return Settings(value != nullptr ? *value : kNone);
    }

    [[nodiscard]] std::span<const JsonValue> Items(std::string_view key) const {
        const JsonValue* value = json_->Find(key);
        return value != nullptr ? value->Items() : std::span<const JsonValue>();
    }

private:
    const JsonValue* json_;
};

CannonShot ShotFrom(const Settings& settings) {
    return {.launch = settings.Point("launch"),
            .target = settings.OptionalPoint("target"),
            .triggerCentre = settings.OptionalPoint("trigger_centre"),
            .triggerRadius = settings.Float("trigger_radius"),
            .event = settings.Event("event")};
}

std::vector<AimPoint> AimsFrom(std::span<const JsonValue> items) {
    std::vector<AimPoint> aims;
    aims.reserve(items.size());
    for (const JsonValue& item : items) {
        const Settings aim(item);
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
            return ShipCannons{.launches = AimsFrom(settings.Items("launches")),
                               .targets = AimsFrom(settings.Items("targets")),
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

HazardPayload HazardPayloadFrom(HazardKind kind, const JsonValue& settings) {
    return PayloadFrom(kind, Settings(settings));
}

}
