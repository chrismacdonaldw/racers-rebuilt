#include "assets/loader_pool.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <vector>

namespace rr::assets {
namespace {

constexpr std::size_t kJobCount = 500;

bool RunsEachIndexOnce(LoaderPool& pool, std::size_t count) {
    std::vector<std::atomic<int>> calls(count);
    pool.ForEach(count, [&](std::size_t index) { calls[index].fetch_add(1); });
    return std::ranges::all_of(calls, [](const std::atomic<int>& call) { return call == 1; });
}

TEST_CASE("each index runs once across the workers") {
    LoaderPool pool(3);
    CHECK(RunsEachIndexOnce(pool, kJobCount));
    CHECK(RunsEachIndexOnce(pool, kJobCount));
}

TEST_CASE("a pool without workers runs every job on the caller") {
    LoaderPool pool(0);
    CHECK(RunsEachIndexOnce(pool, kJobCount));
}

TEST_CASE("an empty batch returns at once") {
    LoaderPool pool(2);
    CHECK(RunsEachIndexOnce(pool, 0));
}

}
}
