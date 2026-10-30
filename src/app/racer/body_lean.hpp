#pragma once

#include <optional>

#include "core/transform.hpp"
#include "core/vector3.hpp"

namespace rr::app {

/// The sprung pitch and roll of a car body over its wheels. It moves only what is drawn.
class BodyLean {
public:
    /// `wheels` and the result are game-space poses. `mass` is the racer's physics mass.
    [[nodiscard]] Transform Update(const Transform& wheels, float elapsedMs, float mass);

private:
    float pitch_ = 0.0F;
    float roll_ = 0.0F;
    std::optional<Vector3> up_;
};

}
