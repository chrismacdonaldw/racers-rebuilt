#include "app/json_fields.hpp"

#include <array>
#include <format>

#include "assets/glb_writer.hpp"

namespace rr::app {
namespace {

std::string Numbers(std::span<const float> values) {
    std::string joined;
    for (const float value : values) {
        joined += (joined.empty() ? "" : ",") + assets::JsonNumber(value);
    }
    return "[" + joined + "]";
}

}

JsonFields& JsonFields::Add(std::string_view key, float value) {
    return AddRaw(key, assets::JsonNumber(value));
}

JsonFields& JsonFields::Add(std::string_view key, std::string_view value) {
    return AddRaw(key, assets::JsonString(value));
}

JsonFields& JsonFields::Add(std::string_view key, const Vector3& value) {
    const std::array values{value.x, value.y, value.z};
    return AddRaw(key, Numbers(values));
}

JsonFields& JsonFields::Add(std::string_view key, const Quaternion& value) {
    const std::array values{value.w, value.x, value.y, value.z};
    return AddRaw(key, Numbers(values));
}

JsonFields& JsonFields::Add(std::string_view key, std::span<const float> values) {
    return AddRaw(key, Numbers(values));
}

JsonFields& JsonFields::Add(std::string_view key, std::span<const std::string> values) {
    std::string joined;
    for (const std::string& value : values) {
        joined += (joined.empty() ? "" : ",") + assets::JsonString(value);
    }
    return AddRaw(key, "[" + joined + "]");
}

JsonFields& JsonFields::Add(std::string_view key, const JsonFields& value) {
    return AddRaw(key, value.Text());
}

JsonFields& JsonFields::AddRaw(std::string_view key, std::string_view json) {
    fields_ += std::format("{}{}:{}", fields_.empty() ? "" : ",", assets::JsonString(key), json);
    return *this;
}

}
