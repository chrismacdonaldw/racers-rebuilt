#include "assets/ldraw_minifig.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "assets/game_path.hpp"

namespace rr::assets {
namespace {

using Rows = std::array<float, 9>;
using Joints = std::array<std::string_view, 4>;
using Bones = std::array<std::string_view, 2>;

constexpr Rows kUnturned{1, 0, 0, 0, 1, 0, 0, 0, 1};
constexpr Rows kSeatedTurn{1, 0, 0, 0, 0, 1, 0, -1, 0};
constexpr Rows kRightArmTurn{0.985F, -0.17F, 0, 0.17F, 0.985F, 0, 0, 0, 1};
constexpr Rows kLeftArmTurn{0.985F, 0.17F, 0, -0.17F, 0.985F, 0, 0, 0, 1};
constexpr Rows kRightHandTurn{0.985F,   -0.1202F, 0.1202F, 0.17F, 0.6964F,
                              -0.6964F, 0,        0.707F,  0.707F};
constexpr Rows kLeftHandTurn{0.985F,   0.1202F, -0.1202F, -0.17F, 0.6964F,
                             -0.6964F, 0,       0.707F,   0.707F};
constexpr Rows kLeftHookTurn{-0.985F,  -0.1202F, -0.1202F, 0.17F, -0.6964F,
                             -0.6964F, 0,        -0.707F,  0.707F};

constexpr Vector3 kHips{.y = 32.0F};
constexpr Vector3 kLegs{.y = 44.0F};
constexpr Vector3 kRightShoulder{.x = -15.552F, .y = 9.0F};
constexpr Vector3 kLeftShoulder{.x = 15.552F, .y = 9.0F};
constexpr Vector3 kRightWrist{.x = -23.6904F, .y = 26.774F, .z = -9.8982F};
constexpr Vector3 kLeftWrist{.x = 23.6904F, .y = 26.774F, .z = -9.8982F};

constexpr std::string_view kTorso = "torso";
constexpr std::string_view kLegsMaterial = "legs";

constexpr LdrawMatrix Placed(const Rows& rows, Vector3 position) {
    return {.position = position,
            .rows = {Vector3{.x = rows[0], .y = rows[1], .z = rows[2]},
                     Vector3{.x = rows[3], .y = rows[4], .z = rows[5]},
                     Vector3{.x = rows[6], .y = rows[7], .z = rows[8]}}};
}

constexpr MinifigPiece Piece(std::string_view file, const LdrawMatrix& placement,
                             std::string_view material, const Joints& joints,
                             std::string_view swingJoint = {}, const Bones& bones = {}) {
    return {.file = file,
            .placement = placement,
            .material = material,
            .joints = joints,
            .swingJoint = swingJoint,
            .bones = bones};
}

constexpr MinifigPiece kStandingTorso =
    Piece("973.dat", Placed(kUnturned, {}), kTorso, {"chest", "waist", kRootJoint});
constexpr MinifigPiece kStandingHips =
    Piece("3815b.dat", Placed(kUnturned, kHips), kLegsMaterial, {kRootJoint});
constexpr MinifigPiece kRightLeg =
    Piece("3816c.dat", Placed(kUnturned, kLegs), kLegsMaterial, {"r_th", "r_kn", "r_an", "r_ba"});
constexpr MinifigPiece kLeftLeg =
    Piece("3817c.dat", Placed(kUnturned, kLegs), kLegsMaterial, {"l_th", "l_kn", "l_an", "l_ba"});
constexpr MinifigPiece kPegLeg =
    Piece("2532.dat", Placed(kUnturned, kLegs), kLegsMaterial, {"r_th", "r_kn", "r_an", "r_ba"});
constexpr MinifigPiece kRightArm = Piece("3818.dat", Placed(kRightArmTurn, kRightShoulder), kTorso,
                                         {"r_sh", "r_el"}, "r_sh", {"r_sh", "r_el"});
constexpr MinifigPiece kLeftArm = Piece("3819.dat", Placed(kLeftArmTurn, kLeftShoulder), kTorso,
                                        {"l_sh", "l_el"}, "l_sh", {"l_sh", "l_el"});
constexpr MinifigPiece kRightHand =
    Piece("3820.dat", Placed(kRightHandTurn, kRightWrist), kTorso, {"r_hand"}, "r_sh", {"r_el"});
constexpr MinifigPiece kLeftHand =
    Piece("3820.dat", Placed(kLeftHandTurn, kLeftWrist), kTorso, {"l_hand"}, "l_sh", {"l_el"});
constexpr MinifigPiece kLeftHook =
    Piece("2531.dat", Placed(kLeftHookTurn, kLeftWrist), kTorso, {"hook"}, "l_sh", {"l_el"});

constexpr std::array kTwoHandsTwoLegs{kStandingTorso, kStandingHips, kRightLeg, kLeftLeg,
                                      kRightArm,      kRightHand,    kLeftArm,  kLeftHand};
constexpr std::array kHookTwoLegs{kStandingTorso, kStandingHips, kRightLeg, kLeftLeg,
                                  kRightArm,      kRightHand,    kLeftArm,  kLeftHook};
constexpr std::array kTwoHandsPegLeg{kStandingTorso, kStandingHips, kPegLeg,  kLeftLeg,
                                     kRightArm,      kRightHand,    kLeftArm, kLeftHand};
constexpr std::array kHookPegLeg{kStandingTorso, kStandingHips, kPegLeg,  kLeftLeg,
                                 kRightArm,      kRightHand,    kLeftArm, kLeftHook};

constexpr std::array kSeatedFigure{
    Piece("973.dat", Placed(kUnturned, {}), kTorso, {"chest"}),
    Piece("3815b.dat", Placed(kUnturned, kHips), kLegsMaterial, {kRootJoint}),
    Piece("3816c.dat", Placed(kSeatedTurn, kLegs), kLegsMaterial, {kRootJoint}),
    Piece("3817c.dat", Placed(kSeatedTurn, kLegs), kLegsMaterial, {kRootJoint}),
    Piece("3818.dat", Placed(kRightArmTurn, kRightShoulder), kTorso, {"arm_r"}, "arm_r"),
    Piece("3820.dat", Placed(kRightHandTurn, kRightWrist), kTorso, {"arm_r"}, "arm_r"),
    Piece("3819.dat", Placed(kLeftArmTurn, kLeftShoulder), kTorso, {"arm_l"}, "arm_l"),
    Piece("3820.dat", Placed(kLeftHandTurn, kLeftWrist), kTorso, {"arm_l"}, "arm_l"),
};

constexpr std::array<std::pair<std::string_view, std::span<const MinifigPiece>>, 5> kFigures{{
    {"rr", kTwoHandsTwoLegs},
    {"hr", kHookTwoLegs},
    {"rp", kTwoHandsPegLeg},
    {"hp", kHookPegLeg},
    {"leg_box", kSeatedFigure},
}};

}

std::span<const MinifigPiece> MinifigFigureOf(std::string_view bodyName) {
    const std::string upper = ToUpperAscii(bodyName);
    const auto found = std::ranges::find_if(
        kFigures, [&](const auto& figure) { return ToUpperAscii(figure.first) == upper; });
    return found == kFigures.end() ? std::span<const MinifigPiece>{} : found->second;
}

}
