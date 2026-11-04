#include "sim/ai/ai_craft.hpp"

#include <algorithm>
#include <array>

namespace rr::sim {
namespace {

struct Skill {
    char letter;
    bool AiCraft::* flag;
};

constexpr std::array kSkills{
    Skill{.letter = 'b', .flag = &AiCraft::brakes}, Skill{.letter = 'p', .flag = &AiCraft::passes},
    Skill{.letter = 'u', .flag = &AiCraft::unsticks}, Skill{.letter = 'a', .flag = &AiCraft::aims}};

}

std::string CraftLetters(const AiCraft& craft) {
    std::string letters;
    for (const Skill& skill : kSkills) {
        if (craft.*skill.flag) {
            letters += skill.letter;
        }
    }
    return letters;
}

std::optional<AiCraft> ParseCraftLetters(std::string_view letters) {
    AiCraft craft;
    for (const char letter : letters) {
        const auto skill = std::ranges::find(kSkills, letter, &Skill::letter);
        if (skill == kSkills.end() || craft.*skill->flag) {
            return std::nullopt;
        }
        craft.*skill->flag = true;
    }
    return letters.empty() ? std::nullopt : std::optional(craft);
}

}
