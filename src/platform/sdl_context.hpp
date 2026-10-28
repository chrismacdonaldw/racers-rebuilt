#pragma once

#include "core/result.hpp"

namespace rr::platform {

/// Owns SDL's video and event subsystems for the lifetime of the app.
///
/// Create exactly one, before any window, and destroy it after every other SDL object.
class SdlContext {
public:
    /// Initializes SDL's video and event subsystems.
    [[nodiscard]] static Result<SdlContext> Create();

    SdlContext(SdlContext&& other) noexcept;
    SdlContext(const SdlContext&) = delete;
    SdlContext& operator=(SdlContext&&) = delete;
    SdlContext& operator=(const SdlContext&) = delete;
    ~SdlContext();

private:
    SdlContext() = default;

    bool isOwner_ = true;
};

}
