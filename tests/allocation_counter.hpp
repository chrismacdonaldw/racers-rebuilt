#pragma once

#include <cstdint>

namespace rr::test {

/// Counts the calling thread's calls to the global `operator new`.
[[nodiscard]] std::uint64_t AllocationCount();

}
