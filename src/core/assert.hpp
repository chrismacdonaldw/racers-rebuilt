#pragma once

#include <source_location>
#include <string_view>

namespace rr {

[[noreturn]] void FailCheck(std::string_view expression, std::source_location location);

}

/// Aborts with a logged message when `condition` is false. Checked in every build.
#define RR_VERIFY(condition)            \
    ((condition) ? static_cast<void>(0) \
                 : ::rr::FailCheck(#condition, std::source_location::current()))

/// Checks `condition` in debug builds only. The condition is not evaluated in release.
#ifdef NDEBUG
#define RR_ASSERT(condition) static_cast<void>(sizeof(condition))
#else
#define RR_ASSERT(condition) RR_VERIFY(condition)
#endif
