#include "app/scene_stack.hpp"

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rr::app {
namespace {

class LoggingScene final : public Scene {
public:
    LoggingScene(std::string name, std::vector<std::string>& events)
        : name_(std::move(name)), events_(events) {}

    void Enter() override { events_.push_back(name_ + " enter"); }
    void Exit() override { events_.push_back(name_ + " exit"); }
    void Tick([[maybe_unused]] const sim::InputFrame& input) override {}
    void Snapshot([[maybe_unused]] sim::RenderSnapshot& out) const override {}
    void Render([[maybe_unused]] const sim::RenderSnapshot& interpolated) override {}

private:
    std::string name_;
    std::vector<std::string>& events_;
};

TEST_CASE("pushes wait for ApplyPendingChanges") {
    std::vector<std::string> events;
    SceneStack stack;

    stack.Push(std::make_unique<LoggingScene>("menu", events));

    CHECK(stack.Top() == nullptr);
    CHECK(stack.ApplyPendingChanges());
    CHECK(stack.Top() != nullptr);
    CHECK(events == std::vector<std::string>{"menu enter"});
}

TEST_CASE("a pop exits the top scene and reveals the one below") {
    std::vector<std::string> events;
    SceneStack stack;
    stack.Push(std::make_unique<LoggingScene>("menu", events));
    stack.Push(std::make_unique<LoggingScene>("race", events));
    static_cast<void>(stack.ApplyPendingChanges());
    const Scene* race = stack.Top();

    stack.Pop();

    CHECK(stack.ApplyPendingChanges());
    CHECK(stack.Top() != race);
    CHECK(events == std::vector<std::string>{"menu enter", "race enter", "race exit"});
}

TEST_CASE("applying with nothing pending reports no change") {
    SceneStack stack;

    CHECK_FALSE(stack.ApplyPendingChanges());
}

}
}
