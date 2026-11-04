#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#include "assets/ldraw_pieces.hpp"

namespace rr::assets {
namespace {

using Axes = std::array<int, 9>;

constexpr Vector3 Row(const Axes& axes, std::size_t row) {
    return {.x = static_cast<float>(axes.at(row * 3)),
            .y = static_cast<float>(axes.at((row * 3) + 1)),
            .z = static_cast<float>(axes.at((row * 3) + 2))};
}

constexpr LdrawPart Part(std::string_view piece, std::string_view file, const Axes& axes,
                         Vector3 offset, std::string_view colour = {}) {
    return {.piece = piece,
            .file = file,
            .matrix = {.position = offset, .rows = {Row(axes, 0), Row(axes, 1), Row(axes, 2)}},
            .colour = colour};
}

constexpr Axes kUnturned{1, 0, 0, 0, 1, 0, 0, 0, 1};
constexpr Axes kQuarter{0, -1, 0, 1, 0, 0, 0, 0, 1};
constexpr Axes kHalf{-1, 0, 0, 0, -1, 0, 0, 0, 1};
constexpr Axes kThreeQuarter{0, 1, 0, -1, 0, 0, 0, 0, 1};
constexpr Axes kGripAlongY{1, 0, 0, 0, 0, -1, 0, 1, 0};
constexpr Axes kGripAlongYTurned{-1, 0, 0, 0, 0, 1, 0, 1, 0};

constexpr std::array kParts{
    Part("L287700", "2877.dat", kQuarter, {.x = 0.5F, .y = 1.0F, .z = 1.2F}),
    Part("L300100", "3001.dat", kQuarter, {.x = 1.0F, .y = 2.0F, .z = 1.2F}),
    Part("L300200", "3002.dat", kQuarter, {.x = 1.0F, .y = 1.5F, .z = 1.2F}),
    Part("L300300", "3003.dat", kUnturned, {.x = 1.0F, .y = 1.0F, .z = 1.2F}),
    Part("L300400", "3004.dat", kQuarter, {.x = 0.5F, .y = 1.0F, .z = 1.2F}),
    Part("L301000", "3010.dat", kQuarter, {.x = 0.5F, .y = 2.0F, .z = 1.2F}),
    Part("L303800", "3038.dat", kUnturned, {.x = 1.5F, .y = 1.5F, .z = 1.2F}),
    Part("L303900", "3039.dat", kUnturned, {.x = 1.0F, .y = 1.5F, .z = 1.2F}),
    Part("L304000", "3040b.dat", kUnturned, {.x = 0.5F, .y = 1.5F, .z = 1.2F}),
    Part("L304500", "3688.dat", kUnturned, {.x = 1.0F, .y = 1.0F, .z = 2.4F}),
    Part("L329700", "3297.dat", kUnturned, {.x = 2.0F, .y = 2.5F, .z = 1.2F}),
    Part("L329800", "3298.dat", kUnturned, {.x = 1.0F, .y = 2.5F, .z = 1.2F}),
    Part("l347500", "3475b.dat", kThreeQuarter, {.x = 0.5F, .y = 2.0F, .z = 1.2F}),
    Part("L362200", "3622.dat", kQuarter, {.x = 0.5F, .y = 1.5F, .z = 1.2F}),
    Part("L395700", "3957a.dat", kUnturned, {.x = 0.5F, .y = 0.5F, .z = 0.4F}),
    Part("L416100", "4161.dat", kUnturned, {.x = 1.5F, .y = 2.5F, .z = 1.2F}),
    Part("L428600", "4286.dat", kUnturned, {.x = 0.5F, .y = 2.5F, .z = 1.2F}),
    Part("L656400", "6564.dat", kHalf, {.x = 1.0F, .y = 1.5F, .z = 1.2F}),
    Part("L656500", "6565.dat", kHalf, {.x = 1.0F, .y = 1.5F, .z = 1.2F}),
    Part("Saxe", "60476.dat", kQuarter, {.x = 0.5F, .y = 3.5F, .z = 1.2F}),
    Part("Saxe", "3848.dat", kGripAlongY, {.x = 1.5F, .y = 4.335F, .z = 0.8F}),
    Part("Saxe1", "60476.dat", kThreeQuarter, {.x = 1.5F, .y = 3.5F, .z = 1.2F}),
    Part("Saxe1", "3848.dat", kGripAlongY, {.x = 0.5F, .y = 4.331F, .z = 0.8F}),
    Part("Sbat", "3004.dat", kHalf, {.x = 2.0F, .y = 0.5F, .z = 1.2F}),
    Part("Sbat", "30103.dat", kHalf, {.x = 2.0F, .y = 0.5F, .z = 1.178F}, "0"),
    Part("s00011", "3665a.dat", kHalf, {.x = 0.5F, .y = 0.5F, .z = 1.2F}),
    Part("s00017", "3023.dat", kThreeQuarter, {.x = 2.5F, .y = 2.0F, .z = 0.4F}),
    Part("s00017", "2570.dat", kThreeQuarter, {.x = 2.5F, .y = 2.0F, .z = 0.4F}, "6"),
    Part("s395700", "3957a.dat", kUnturned, {.x = 0.5F, .y = 0.5F, .z = 0.4F}, "0"),
    Part("s395700", "2335.dat", kThreeQuarter, {.x = 0.5F, .y = 0.5F, .z = 4.4F}),
    Part("sdsrtax1", "60476.dat", kQuarter, {.x = 0.5F, .y = 2.5F, .z = 2.4F}),
    Part("sdsrtax1", "3841.dat", {-1, 0, 0, 0, 0, -1, 0, -1, 0},
         {.x = 1.5F, .y = 2.788F, .z = 2.0F}, "71"),
    Part("sdsrtax2", "60476.dat", kThreeQuarter, {.x = 1.5F, .y = 2.5F, .z = 2.4F}),
    Part("sdsrtax2", "3841.dat", {-1, 0, 0, 0, 0, -1, 0, -1, 0},
         {.x = 0.5F, .y = 2.789F, .z = 2.0F}, "71"),
    Part("sFlag", "3957a.dat", kUnturned, {.x = 0.5F, .y = 0.5F, .z = 0.4F}, "72"),
    Part("sFlag", "4495a.dat", kThreeQuarter, {.x = 0.45F, .y = 0.5F, .z = 4.4F}),
    Part("sLance", "60476.dat", kQuarter, {.x = 0.5F, .y = 1.5F, .z = 1.2F}),
    Part("sLance", "3849.dat", {0, 0, 1, 0, -1, 0, 1, 0, 0}, {.x = 1.5F, .y = 1.452F, .z = 0.8F},
         "71"),
    Part("sLance01", "60476.dat", kThreeQuarter, {.x = 1.5F, .y = 1.5F, .z = 1.2F}),
    Part("sLance01", "3849.dat", {0, 0, -1, 0, -1, 0, -1, 0, 0},
         {.x = 0.5F, .y = 1.471F, .z = 0.8F}, "71"),
    Part("sprtfnce", "30055.dat", kThreeQuarter, {.x = 0.5F, .y = 2.0F, .z = 2.4F}, "0"),
    Part("sprtoar", "60476.dat", kQuarter, {.x = 0.5F, .y = 2.5F, .z = 1.2F}),
    Part("sprtoar", "2542.dat", {0, -1, 0, 0, 0, -1, 1, 0, 0}, {.x = 1.5F, .y = 0.391F, .z = 0.8F},
         "6"),
    Part("sprtoar2", "60476.dat", kThreeQuarter, {.x = 1.5F, .y = 2.5F, .z = 1.2F}),
    Part("sprtoar2", "2542.dat", {0, -1, 0, 0, 0, -1, 1, 0, 0}, {.x = 0.5F, .y = 0.388F, .z = 0.8F},
         "6"),
    Part("sprtswd", "60476.dat", kQuarter, {.x = 0.5F, .y = 0.5F, .z = 1.2F}),
    Part("sprtswd", "2530.dat", kGripAlongYTurned, {.x = 1.5F, .y = 0.71F, .z = 0.8F}, "72"),
    Part("sprtswd2", "60476.dat", kThreeQuarter, {.x = 1.5F, .y = 0.5F, .z = 1.2F}),
    Part("sprtswd2", "2530.dat", kGripAlongYTurned, {.x = 0.5F, .y = 0.71F, .z = 0.8F}, "72"),
    Part("srifle02", "60475b.dat", kThreeQuarter, {.x = 1.5F, .y = 2.5F, .z = 1.2F}),
    Part("srifle02", "30141.dat", kHalf, {.x = 0.5F, .y = 2.4F, .z = 0.494F}, "0"),
    Part("srifle1", "60475b.dat", kQuarter, {.x = 0.5F, .y = 2.5F, .z = 1.2F}),
    Part("srifle1", "30141.dat", kHalf, {.x = 1.5F, .y = 2.4F, .z = 0.494F}, "0"),
    Part("sspider", "3024.dat", kQuarter, {.x = 1.5F, .y = 1.5F, .z = 0.4F}),
    Part("sspider", "30238.dat", kHalf, {.x = 1.5F, .y = 1.405F, .z = 0.073F}, "0"),
    Part("sSWORD", "60476.dat", kQuarter, {.x = 0.5F, .y = 1.5F, .z = 2.4F}),
    Part("sSWORD", "3847.dat", kGripAlongYTurned, {.x = 1.5F, .y = 2.551F, .z = 2.0F}, "15"),
    Part("sSWORD01", "60476.dat", kThreeQuarter, {.x = 1.5F, .y = 1.5F, .z = 2.4F}),
    Part("sSWORD01", "3847.dat", {1, 0, 0, 0, 0, 1, 0, -1, 0}, {.x = 0.5F, .y = 2.551F, .z = 2.0F},
         "15"),
    Part("SWand", "3004.dat", kUnturned, {.x = 1.0F, .y = 0.5F, .z = 1.2F}),
    Part("SWand", "6124b.dat", kUnturned, {.x = 1.0F, .y = 0.5F, .z = 0.989F}, "0"),
};

}

std::span<const LdrawPart> LdrawParts() {
    return kParts;
}

}
