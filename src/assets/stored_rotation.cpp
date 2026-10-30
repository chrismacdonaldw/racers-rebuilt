#include "assets/stored_rotation.hpp"

#include <cmath>

namespace rr::assets {

Result<Quaternion> FromStoredRotation(const StoredRotation& stored) {
    const auto [x, y, z, w] = stored;
    const float length = std::sqrt((x * x) + (y * y) + (z * z) + (w * w));
    if (!(length > 0.0F)) {
        return Fail(ErrorCode::InvalidData, "rotation has zero length");
    }
    return Quaternion{.w = w / length, .x = -x / length, .y = -y / length, .z = -z / length};
}

}
