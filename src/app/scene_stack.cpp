#include "app/scene_stack.hpp"

#include <utility>

#include "core/assert.hpp"

namespace rr::app {

void SceneStack::Push(std::unique_ptr<Scene> scene) {
    RR_ASSERT(scene != nullptr);
    pending_.push_back({.kind = ChangeKind::Push, .scene = std::move(scene)});
}

void SceneStack::Pop() {
    pending_.push_back({.kind = ChangeKind::Pop, .scene = nullptr});
}

bool SceneStack::ApplyPendingChanges() {
    if (pending_.empty()) {
        return false;
    }

    const Scene* previousTop = Top();
    for (PendingChange& change : pending_) {
        if (change.kind == ChangeKind::Push) {
            scenes_.push_back(std::move(change.scene));
            scenes_.back()->Enter();
        } else {
            RR_VERIFY(!scenes_.empty());
            scenes_.back()->Exit();
            scenes_.pop_back();
        }
    }
    pending_.clear();
    ++changeCount_;
    return Top() != previousTop;
}

Scene* SceneStack::Top() const {
    return scenes_.empty() ? nullptr : scenes_.back().get();
}

}
