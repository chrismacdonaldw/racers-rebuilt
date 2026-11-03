#pragma once

#include <cstdint>

#include "sim/input_frame.hpp"

namespace rr::app {

enum class LicenceRow : std::uint8_t { Name, Expression, Done, Back };

enum class LicenceRequest : std::uint8_t { None, Expression, Done, Leave };

class LicenceMenu {
public:
    LicenceRequest Tick(const sim::InputFrame& input);

    [[nodiscard]] LicenceRow Focus() const { return focus_; }

private:
    LicenceRow focus_ = LicenceRow::Name;
};

}
