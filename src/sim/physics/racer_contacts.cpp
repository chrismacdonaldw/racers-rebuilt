#include "sim/physics/racer_contacts.hpp"

#include <numeric>
#include <optional>

#include "sim/ai/ai_racer.hpp"
#include "sim/physics/rigid_body.hpp"
#include "sim/physics/wall_sweep.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kBoxHalfSize{.x = 4.0F, .y = 2.5F, .z = 3.1F};
constexpr float kRestitution = 0.75F;

float MinX(const ContactBody& entry) {
    return entry.body->motion.position.x - entry.constants->boundsRadius;
}

float MaxX(const ContactBody& entry) {
    return entry.body->motion.position.x + entry.constants->boundsRadius;
}

bool CanTouch(const ContactBody& a, const ContactBody& b) {
    return a.isSolid && b.isSolid && a.boxScale > 0.0F && b.boxScale > 0.0F;
}

bool SpheresOverlap(const ContactBody& a, const ContactBody& b) {
    const Vector3 offset = a.body->motion.position - b.body->motion.position;
    const float reach = a.constants->boundsRadius + b.constants->boundsRadius;
    return Dot(offset, offset) < reach * reach;
}

Vector3 InverseInertia(const ContactBody& entry, const Vector3& v) {
    const Basis& basis = entry.body->motion.basis;
    const Vector3& inertia = entry.constants->inertia;
    const Vector3 local = ToBody(basis, v);
    return ToWorld(basis,
                   {.x = local.x / inertia.x, .y = local.y / inertia.y, .z = local.z / inertia.z});
}

void MoveOutOfContact(const ContactBody& entry, const Vector3& offset) {
    if (entry.route != nullptr) {
        entry.route->Shift(offset);
        entry.body->motion.position = entry.route->Pose().position;
        return;
    }
    BodyState moved = *entry.body;
    moved.saved = entry.body->motion;
    moved.motion.position += offset;
    if (!SweepBodyPoints(moved, *entry.constants, *entry.world)) {
        entry.body->motion.position = moved.motion.position;
    }
}

float AngularTerm(const ContactBody& entry, const Vector3& point, const Vector3& normal) {
    const Vector3 centre = CentreOfMassWorld(entry.body->motion, *entry.constants);
    const Vector3 lever = Normalized(centre - point, normal);
    return Dot(normal, Cross(InverseInertia(entry, Cross(lever, normal)), lever));
}

void ApplyImpulse(const ContactBody& entry, const Vector3& normal, float impulse) {
    if (entry.route != nullptr) {
        entry.route->Bump(normal, impulse);
    } else {
        entry.body->motion.velocity += normal * (impulse / entry.constants->mass);
    }
}

}

ContactBox BoxOf(const BodyState& body, const BodyConstants& constants, float scale) {
    return {.centre = CentreOfMassWorld(body.motion, constants),
            .axes = body.motion.basis,
            .halfSize = kBoxHalfSize * scale};
}

void RespondToContact(const ContactBody& first, const ContactBody& second,
                      const BoxContact& contact) {
    const Vector3& normal = contact.normal;
    MoveOutOfContact(first, normal * contact.penetration);
    const float firstMass = first.constants->mass;
    const float secondMass = second.constants->mass;
    const float resistance = (1.0F / firstMass) + (1.0F / secondMass) +
                             AngularTerm(first, contact.point, normal) +
                             AngularTerm(second, contact.point, normal);
    const Vector3 closing = first.body->motion.velocity - second.body->motion.velocity;
    const float impulse = -(1.0F + kRestitution) * Dot(closing, normal) / resistance;
    ApplyImpulse(first, normal, impulse);
    ApplyImpulse(second, normal, -impulse);
}

void RacerContacts::Resolve(std::span<const ContactBody> bodies) {
    SortByMinX(bodies);
    contacts_.clear();
    for (std::size_t i = 0; i < order_.size(); ++i) {
        const ContactBody& first = bodies[order_[i]];
        for (std::size_t j = i + 1; j < order_.size(); ++j) {
            const ContactBody& second = bodies[order_[j]];
            if (MaxX(first) < MinX(second)) {
                break;
            }
            if (!CanTouch(first, second) || !SpheresOverlap(first, second)) {
                continue;
            }
            const std::optional<BoxContact> contact =
                TestBoxes(BoxOf(*first.body, *first.constants, first.boxScale),
                          BoxOf(*second.body, *second.constants, second.boxScale));
            if (contact) {
                RespondToContact(first, second, *contact);
                contacts_.push_back(
                    {.first = order_[i], .second = order_[j], .point = contact->point});
            }
        }
    }
}

void RacerContacts::SortByMinX(std::span<const ContactBody> bodies) {
    if (order_.size() != bodies.size()) {
        order_.resize(bodies.size());
        std::ranges::iota(order_, std::size_t{0});
        contacts_.reserve(bodies.size() * bodies.size() / 2);
    }
    for (std::size_t i = 1; i < order_.size(); ++i) {
        const std::size_t moving = order_[i];
        const float key = MinX(bodies[moving]);
        std::size_t slot = i;
        while (slot > 0 && MinX(bodies[order_[slot - 1]]) > key) {
            order_[slot] = order_[slot - 1];
            --slot;
        }
        order_[slot] = moving;
    }
}

}
