#include "app/cutscene/cutscene_timeline.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace rr::app {
namespace {

constexpr std::size_t kMaxDirectionalLights = 7;
constexpr std::int64_t kMillisecondsPerSecond = 1000;
constexpr float kBlinkFrameMs = 33.333328F;

Milliseconds BlinkLength(std::int32_t frames) {
    return Milliseconds{static_cast<std::int64_t>(static_cast<float>(frames) * kBlinkFrameMs)};
}

template <typename Event>
void AddEvents(const std::vector<Event>& events, CutsceneEventKind kind, auto& out) {
    for (std::size_t index = 0; index < events.size(); ++index) {
        const assets::CutsceneSpan& span = events[index].span;
        out.push_back(
            {.id = {.kind = kind, .index = index}, .start = span.start, .end = span.End()});
    }
}

}

CutsceneTimeline::CutsceneTimeline(const assets::CutsceneFrame& frame, bool isLooping)
    : frame_(&frame), isLooping_(isLooping) {
    AddEvents(frame.cameras, CutsceneEventKind::Camera, events_);
    AddEvents(frame.models, CutsceneEventKind::Model, events_);
    AddEvents(frame.transforms, CutsceneEventKind::Transform, events_);
    AddEvents(frame.ambientLights, CutsceneEventKind::AmbientLight, events_);
    AddEvents(frame.directionalLights, CutsceneEventKind::DirectionalLight, events_);
    for (std::size_t event = 0; event < events_.size(); ++event) {
        byStart_.push_back(event);
        byEnd_.push_back(event);
    }
    std::ranges::stable_sort(byStart_, {}, [&](std::size_t event) { return events_[event].start; });
    std::ranges::stable_sort(byEnd_, {}, [&](std::size_t event) { return events_[event].end; });
    blinks_.resize(events_.size());
    for (std::size_t event = 0; event < events_.size(); ++event) {
        const CutsceneEventId id = events_[event].id;
        std::optional<assets::LightBlink> blink;
        if (id.kind == CutsceneEventKind::AmbientLight) {
            blink = frame.ambientLights[id.index].blink;
        } else if (id.kind == CutsceneEventKind::DirectionalLight) {
            blink = frame.directionalLights[id.index].blink;
        }
        if (blink) {
            blinks_[event] =
                Blink{.on = BlinkLength(blink->framesOn), .off = BlinkLength(blink->framesOff)};
        }
    }
}

std::span<const CutsceneEventChange> CutsceneTimeline::Advance(Milliseconds elapsed) {
    changes_.clear();
    if (isComplete_) {
        return changes_;
    }
    scaled_ += frame_->framesPerSecond * elapsed.count();
    std::int64_t frame = scaled_ / kMillisecondsPerSecond;
    if (frame >= frame_->length) {
        ProcessEvents(currentFrame_, frame);
        if (!isLooping_) {
            isComplete_ = true;
            currentFrame_ = frame;
            return changes_;
        }
        RewindEvents();
        frame = 0;
    }
    ProcessEvents(currentFrame_, frame);
    UpdateActive(elapsed);
    currentFrame_ = frame;
    return changes_;
}

std::span<const CutsceneEventChange> CutsceneTimeline::Rewind() {
    changes_.clear();
    RewindEvents();
    isComplete_ = false;
    return changes_;
}

bool CutsceneTimeline::IsActive(CutsceneEventId event) const {
    const auto found = std::ranges::find(events_, event, &TimedEvent::id);
    return found != events_.end() && found->isActive;
}

std::optional<std::size_t> CutsceneTimeline::DrawingCamera() const {
    if (cameras_.empty()) {
        return std::nullopt;
    }
    return events_[cameras_.front()].id.index;
}

std::optional<std::size_t> CutsceneTimeline::NewestCamera() const {
    if (cameras_.empty()) {
        return std::nullopt;
    }
    return events_[cameras_.back()].id.index;
}

void CutsceneTimeline::ProcessEvents(std::int64_t from, std::int64_t to) {
    const auto isDue = [&](std::int32_t frame) { return frame >= from && frame <= to; };
    while (true) {
        const bool hasStart = nextStart_ < byStart_.size();
        const bool hasEnd = nextEnd_ < byEnd_.size();
        const bool beginsFirst = hasStart && (!hasEnd || events_[byStart_[nextStart_]].start <=
                                                             events_[byEnd_[nextEnd_]].end);
        if (beginsFirst) {
            if (!isDue(events_[byStart_[nextStart_]].start)) {
                return;
            }
            Begin(byStart_[nextStart_++]);
        } else {
            if (!hasEnd || !isDue(events_[byEnd_[nextEnd_]].end)) {
                return;
            }
            End(byEnd_[nextEnd_++]);
        }
    }
}

void CutsceneTimeline::Begin(std::size_t event) {
    TimedEvent& timed = events_[event];
    if (timed.isActive) {
        return;
    }
    timed.isActive = true;
    active_.push_back(event);
    if (timed.id.kind == CutsceneEventKind::Camera) {
        const std::string& camera = frame_->cameras[timed.id.index].camera;
        if (std::ranges::none_of(cameras_, [&](std::size_t pushed) {
                return frame_->cameras[events_[pushed].id.index].camera == camera;
            })) {
            cameras_.push_back(event);
        }
    }
    if (std::optional<Blink>& blink = blinks_[event]) {
        blink->left = blink->on;
        blink->isLit = true;
    }
    Light(timed, true);
    changes_.push_back({.event = timed.id, .begins = true});
}

void CutsceneTimeline::End(std::size_t event) {
    TimedEvent& timed = events_[event];
    if (!timed.isActive) {
        return;
    }
    timed.isActive = false;
    std::erase(active_, event);
    if (timed.id.kind == CutsceneEventKind::Camera) {
        const std::string& camera = frame_->cameras[timed.id.index].camera;
        const auto pushed = std::ranges::find_if(cameras_, [&](std::size_t other) {
            return frame_->cameras[events_[other].id.index].camera == camera;
        });
        if (pushed != cameras_.end()) {
            cameras_.erase(pushed);
        }
    }
    Light(timed, false);
    changes_.push_back({.event = timed.id, .begins = false});
}

void CutsceneTimeline::UpdateActive(Milliseconds elapsed) {
    for (const std::size_t event : std::views::reverse(active_)) {
        UpdateBlink(event, elapsed);
    }
}

void CutsceneTimeline::UpdateBlink(std::size_t event, Milliseconds elapsed) {
    std::optional<Blink>& blink = blinks_[event];
    if (!blink) {
        return;
    }
    if (blink->left >= elapsed) {
        blink->left -= elapsed;
        return;
    }
    blink->isLit = !blink->isLit;
    blink->left = blink->isLit ? blink->on : blink->off;
    Light(events_[event], blink->isLit);
}

void CutsceneTimeline::Light(const TimedEvent& event, bool isLit) {
    const std::size_t index = event.id.index;
    if (event.id.kind == CutsceneEventKind::AmbientLight) {
        if (isLit) {
            ambient_ = index;
        } else if (ambient_ == index) {
            ambient_.reset();
        }
    } else if (event.id.kind == CutsceneEventKind::DirectionalLight) {
        const bool isListed = std::ranges::contains(lights_, index);
        if (isLit && !isListed) {
            if (lights_.size() >= kMaxDirectionalLights) {
                lights_.erase(lights_.begin());
            }
            lights_.push_back(index);
        } else if (!isLit) {
            std::erase(lights_, index);
        }
    }
}

void CutsceneTimeline::RewindEvents() {
    scaled_ = 0;
    currentFrame_ = 0;
    nextStart_ = 0;
    nextEnd_ = 0;
    while (!active_.empty()) {
        End(active_.back());
    }
}

}
