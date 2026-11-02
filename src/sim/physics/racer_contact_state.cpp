#include <algorithm>

#include "sim/physics/racer.hpp"

namespace rr::sim {
namespace {

constexpr int kBoxShrinkMs = 750;
constexpr int kBoxGoneMs = 500;
constexpr float kBoxScalePerMs = 0.004F;
constexpr int kScrapeCooldownMs = 250;

}

ContactBody Racer::ForContacts(const CollisionQuery& world) {
    const bool isSpunOut = route_ ? route_->Effects().isSpunOut : body_.isSpunOut;
    return {.body = &body_,
            .constants = &constants_,
            .world = &world,
            .route = route_ ? &*route_ : nullptr,
            .isSolid = !status_.isGhost && !isSpunOut,
            .boxScale = status_.contact.boxScale};
}

void Racer::ShrinkBox() {
    if (status_.contact.boxScale == 1.0F) {
        status_.contact.boxMs = kBoxShrinkMs;
    }
}

void Racer::MarkTouching() {
    status_.contact.isTouching = true;
}

void Racer::Scrape(std::uint8_t sound) {
    events_.scrapeSound = sound;
    HoldScrape();
}

void Racer::Spark(const Vector3& point) {
    events_.sparkPoint = point;
}

void Racer::HoldScrape() {
    status_.contact.scrapeCooldownMs = kScrapeCooldownMs;
}

void Racer::UpdateContactTimers(int elapsedMs) {
    ContactStatus& contact = status_.contact;
    contact.scrapeCooldownMs = std::max(0, contact.scrapeCooldownMs - elapsedMs);
    if (contact.isTouching && contact.boxMs > elapsedMs) {
        contact.boxMs -= elapsedMs;
        contact.boxScale = contact.boxMs > kBoxGoneMs
                               ? static_cast<float>(contact.boxMs - kBoxGoneMs) * kBoxScalePerMs
                               : 0.0F;
    } else {
        contact.boxMs = 0;
        contact.boxScale = 1.0F;
    }
    contact.isTouching = false;
}

}
