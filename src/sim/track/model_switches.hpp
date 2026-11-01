#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rr::sim {

/// Names are entity names, which the app matches to their draws.
class ModelSwitches {
public:
    [[nodiscard]] std::size_t Add(std::string_view entity);
    void SetHidden(std::size_t model, bool isHidden) { hidden_.at(model) = isHidden; }

    [[nodiscard]] std::span<const std::string> Names() const { return names_; }
    [[nodiscard]] const std::vector<bool>& Hidden() const { return hidden_; }

private:
    std::vector<std::string> names_;
    std::vector<bool> hidden_;
};

}
