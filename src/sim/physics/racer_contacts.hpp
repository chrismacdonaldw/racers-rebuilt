#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "core/vector3.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/physics/box_contact.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/physics/collision_query.hpp"

namespace rr::sim {

class AiRacer;

/// A body with a `route` takes contacts through it, and its `body` only mirrors the route's pose.
struct ContactBody {
    BodyState* body = nullptr;
    const BodyConstants* constants = nullptr;
    const CollisionQuery* world = nullptr;
    AiRacer* route = nullptr;
    bool isSolid = true;
    float boxScale = 1.0F;
};

[[nodiscard]] ContactBox BoxOf(const BodyState& body, const BodyConstants& constants,
                               float scale = 1.0F);

/// Pushes only `first` out of the overlap, unless that would put a body point through a wall, then
/// exchanges a linear impulse.
void RespondToContact(const ContactBody& first, const ContactBody& second,
                      const BoxContact& contact);

struct RacerContact {
    std::size_t first = 0;
    std::size_t second = 0;
    Vector3 point;
};

/// Run once per tick after every racer has stepped. Bodies must keep their index between ticks.
class RacerContacts {
public:
    void Resolve(std::span<const ContactBody> bodies);

    /// The pairs the last `Resolve` responded to, as indices into its bodies.
    [[nodiscard]] std::span<const RacerContact> Contacts() const { return contacts_; }

private:
    void SortByMinX(std::span<const ContactBody> bodies);

    std::vector<std::size_t> order_;
    std::vector<RacerContact> contacts_;
};

}
