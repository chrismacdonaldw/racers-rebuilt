#pragma once

#include <concepts>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "core/quaternion.hpp"
#include "core/vector3.hpp"

namespace rr::app {

/// Builds one JSON object. Vectors and rotations stay in game space, as `[x,y,z]` and `[w,x,y,z]`.
class JsonFields {
public:
    template <std::integral Value>
    JsonFields& Add(std::string_view key, Value value) {
        return AddRaw(key, std::to_string(value));
    }

    JsonFields& Add(std::string_view key, float value);
    JsonFields& Add(std::string_view key, std::string_view value);
    JsonFields& Add(std::string_view key, const Vector3& value);
    JsonFields& Add(std::string_view key, const Quaternion& value);
    JsonFields& Add(std::string_view key, std::span<const float> values);
    JsonFields& Add(std::string_view key, std::span<const std::string> values);
    JsonFields& Add(std::string_view key, const JsonFields& value);

    template <typename Value>
    JsonFields& Add(std::string_view key, const std::optional<Value>& value) {
        return value ? Add(key, *value) : *this;
    }

    JsonFields& AddRaw(std::string_view key, std::string_view json);

    [[nodiscard]] std::string Text() const { return "{" + fields_ + "}"; }

    [[nodiscard]] bool IsEmpty() const { return fields_.empty(); }

private:
    std::string fields_;
};

}
