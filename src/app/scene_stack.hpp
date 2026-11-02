#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "app/scene.hpp"

namespace rr::app {

/// Push and Pop are deferred to `ApplyPendingChanges`. Only the top scene runs.
class SceneStack {
public:
    void Push(std::unique_ptr<Scene> scene);
    void Pop();

    /// Returns true when the top scene changed.
    bool ApplyPendingChanges();

    [[nodiscard]] Scene* Top() const;

    /// Counts applied changes, so a new top scene at a reused address still counts as new.
    [[nodiscard]] std::uint64_t ChangeCount() const { return changeCount_; }

private:
    enum class ChangeKind : std::uint8_t {
        Push,
        Pop,
    };

    struct PendingChange {
        ChangeKind kind;
        std::unique_ptr<Scene> scene;
    };

    std::vector<std::unique_ptr<Scene>> scenes_;
    std::vector<PendingChange> pending_;
    std::uint64_t changeCount_ = 0;
};

}
