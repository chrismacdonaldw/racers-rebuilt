#include "sim/track/model_switches.hpp"

namespace rr::sim {

std::size_t ModelSwitches::Add(std::string_view entity) {
    names_.emplace_back(entity);
    hidden_.push_back(false);
    return names_.size() - 1;
}

}
