#include "assets/asset_cache.hpp"

#include <doctest/doctest.h>

#include <string>

namespace rr::assets {
namespace {

TEST_CASE("an inserted asset is found by its path") {
    AssetCache<std::string> cache;
    const AssetCache<std::string>::Handle inserted = cache.Insert("A/B.GDB", "model");

    CHECK(cache.Find("A/B.GDB") == inserted);
    CHECK(cache.Find("A/C.GDB") == nullptr);
}

TEST_CASE("pruning keeps only assets that are still held") {
    AssetCache<std::string> cache;
    const AssetCache<std::string>::Handle held = cache.Insert("HELD", "held");
    cache.Insert("DROPPED", "dropped");

    cache.Prune();

    CHECK(cache.Size() == 1);
    CHECK(cache.Find("HELD") == held);
    CHECK(cache.Find("DROPPED") == nullptr);
}

}
}
