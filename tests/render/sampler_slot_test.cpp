#include "render/sampler_slot.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <set>

namespace rr::render {

TEST_CASE("anisotropic linear sampling gets its own slots and point sampling ignores it") {
    std::set<std::size_t> slots;
    for (const TextureAddress address : {TextureAddress::Repeat, TextureAddress::Clamp}) {
        slots.insert(SamplerSlot(TextureFilter::Linear, address, false));
        slots.insert(SamplerSlot(TextureFilter::Linear, address, true));
        slots.insert(SamplerSlot(TextureFilter::Point, address, false));
        CHECK(SamplerSlot(TextureFilter::Point, address, true) ==
              SamplerSlot(TextureFilter::Point, address, false));
    }

    CHECK(slots.size() == kSamplerCount);
    CHECK(*slots.rbegin() == kSamplerCount - 1);
}

}
