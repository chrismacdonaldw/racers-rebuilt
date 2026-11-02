#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "assets/cutscene_timeline.hpp"
#include "core/duration.hpp"

namespace rr::app {

enum class CutsceneEventKind : std::uint8_t {
    Camera,
    Model,
    Transform,
    AmbientLight,
    DirectionalLight
};

/// `index` is into the frame's list for `kind`.
struct CutsceneEventId {
    CutsceneEventKind kind = CutsceneEventKind::Camera;
    std::size_t index = 0;

    friend bool operator==(const CutsceneEventId&, const CutsceneEventId&) = default;
};

struct CutsceneEventChange {
    CutsceneEventId event;
    bool begins = false;

    friend bool operator==(const CutsceneEventChange&, const CutsceneEventChange&) = default;
};

/// Plays one `CDB` frame on its frame clock, without drawing. The frame must outlive it.
class CutsceneTimeline {
public:
    explicit CutsceneTimeline(const assets::CutsceneFrame& frame, bool isLooping = false);

    /// Returns the events that began or ended, in order, valid until the next call.
    std::span<const CutsceneEventChange> Advance(Milliseconds elapsed);

    /// Ends every active event, as a loop does, and starts the clock again.
    std::span<const CutsceneEventChange> Rewind();

    [[nodiscard]] bool IsComplete() const { return isComplete_; }
    [[nodiscard]] bool IsActive(CutsceneEventId event) const;

    /// The oldest active camera event, which the view draws through.
    [[nodiscard]] std::optional<std::size_t> DrawingCamera() const;

    /// The newest active camera event, which screens fade over.
    [[nodiscard]] std::optional<std::size_t> NewestCamera() const;

    /// Unset when no ambient event is active or its blink is off.
    [[nodiscard]] std::optional<std::size_t> AmbientLight() const { return ambient_; }

    /// Lit directional light events, oldest first, at most 7.
    [[nodiscard]] std::span<const std::size_t> DirectionalLights() const { return lights_; }

private:
    struct TimedEvent {
        CutsceneEventId id;
        std::int32_t start = 0;
        std::int32_t end = 0;
        bool isActive = false;
    };

    struct Blink {
        Milliseconds on{};
        Milliseconds off{};
        Milliseconds left{};
        bool isLit = false;
    };

    void ProcessEvents(std::int64_t from, std::int64_t to);
    void Begin(std::size_t event);
    void End(std::size_t event);
    void UpdateActive(Milliseconds elapsed);
    void UpdateBlink(std::size_t event, Milliseconds elapsed);
    void Light(const TimedEvent& event, bool isLit);
    void RewindEvents();

    const assets::CutsceneFrame* frame_;
    bool isLooping_;
    std::vector<TimedEvent> events_;
    std::vector<std::size_t> byStart_;
    std::vector<std::size_t> byEnd_;
    std::vector<std::optional<Blink>> blinks_;
    std::size_t nextStart_ = 0;
    std::size_t nextEnd_ = 0;
    std::int64_t scaled_ = 0;
    std::int64_t currentFrame_ = 0;
    bool isComplete_ = false;
    std::vector<std::size_t> active_;
    std::vector<std::size_t> cameras_;
    std::optional<std::size_t> ambient_;
    std::vector<std::size_t> lights_;
    std::vector<CutsceneEventChange> changes_;
};

}
