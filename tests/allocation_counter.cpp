#include "allocation_counter.hpp"

#include <cstddef>
#include <cstdlib>
#include <new>

namespace {

thread_local std::uint64_t allocations = 0;

}

std::uint64_t rr::test::AllocationCount() {
    return allocations;
}

// NOLINTNEXTLINE(misc-new-delete-overloads)
void* operator new(std::size_t size) {
    ++allocations;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr) {
        throw std::bad_alloc();
    }
    return memory;
}
