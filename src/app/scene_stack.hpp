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
};

}
