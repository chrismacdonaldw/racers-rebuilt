#include "app/free_camera.hpp"

#include <doctest/doctest.h>

#include <cmath>

#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr Transform kSceneCamera{.position = {.y = 3.0F, .z = 8.0F}};
constexpr FloatSeconds kOneSecond{1.0};

class CameraFixture {
public:
    CameraFixture() : camera_([this] { return keys_; }) {
        camera_.DeclareSettings(settings_);
        camera_.Attach(hooks_, settings_);
    }

    Transform Render() {
        Transform view = kSceneCamera;
        hooks_.Emit(mods::FrameRendering{.camera = view, .elapsed = kOneSecond});
        return view;
    }

    void Enable(bool isEnabled) {
        settings_.Set(kFreeCamera, isEnabled);
        hooks_.Emit(mods::SettingChanged{.key = kFreeCamera.key, .settings = settings_});
    }

protected:
    FlyKeys keys_;
    settings::Registry settings_;
    mods::HookRegistry hooks_;
    FreeCamera camera_;
};

TEST_CASE_FIXTURE(CameraFixture, "the free camera is off by default") {
    keys_.forward = true;

    CHECK(Render() == kSceneCamera);
}

TEST_CASE_FIXTURE(CameraFixture, "turning it on starts from the scene camera") {
    Enable(true);

    const Transform view = Render();

    CHECK(view.position == kSceneCamera.position);
    CHECK(std::abs(Dot(view.rotation, kSceneCamera.rotation)) == doctest::Approx(1.0F));
}

TEST_CASE_FIXTURE(CameraFixture, "flying forward moves along the view and keeps going") {
    Enable(true);
    keys_.forward = true;

    CHECK(Render().position.z == doctest::Approx(8.0F - kFreeCameraSpeed.defaultValue));
    CHECK(Render().position.z == doctest::Approx(8.0F - (2.0 * kFreeCameraSpeed.defaultValue)));
}

TEST_CASE_FIXTURE(CameraFixture, "the speed setting sets how far it flies each second") {
    Enable(true);
    settings_.Set(kFreeCameraSpeed, 5.0);
    hooks_.Emit(mods::SettingChanged{.key = kFreeCameraSpeed.key, .settings = settings_});
    keys_.forward = true;

    CHECK(Render().position.z == doctest::Approx(3.0F));
}

TEST_CASE_FIXTURE(CameraFixture, "a new scene puts it back on the scene camera") {
    Enable(true);
    keys_.up = true;
    static_cast<void>(Render());
    keys_.up = false;

    hooks_.Emit(mods::SceneEntered{});

    CHECK(Render().position == kSceneCamera.position);
}

}
}
