#include "sim/powerup/warp_rules.hpp"

namespace rr::sim {
namespace {

constexpr WarpPace kFairPace{
    .walkPerMs = 0.36F, .routeSpeed = 3.0F, .launch = WarpLaunch::TopSpeed};
constexpr float kDrivingPerMs = 0.12F;
constexpr float kLeaderShare = 1.0F / 3.0F;

float Shared(float speed, float driving) {
    return driving + ((speed - driving) * kLeaderShare);
}

}

WarpPace PaceFor(const WarpRules& rules, const WarpPace& original, bool isLeading) {
    WarpPace pace = rules.isFair ? kFairPace : original;
    if (rules.catchesUp && isLeading) {
        pace.walkPerMs = Shared(pace.walkPerMs, kDrivingPerMs);
        pace.routeSpeed = Shared(pace.routeSpeed, 1.0F);
    }
    return pace;
}

}
