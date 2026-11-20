#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/cam.hpp>

using namespace fuhgl;
using namespace fuhgl::math;

namespace {

bool matNear(const Mat4& a, const Mat4& b, float eps = 1e-5f) {
    for (int i = 0; i < 16; ++i)
        if (std::fabs(a.m[i] - b.m[i]) > eps) return false;
    return true;
}

} // namespace

void runCameraTests() {
    // --- first person: at origin, yaw=pitch=0 -> identity view ---
    {
        FirstPersonCamera cam;
        cam.position = {0.f, 0.f, 0.f};
        cam.yaw = 0.f;
        cam.pitch = 0.f;
        CHECK(matNear(cam.view(), Mat4{}));
        CHECK(cam.forward() == Vec3(0.f, 0.f, -1.f));

        // hold W for 1s with moveSpeed 5 -> moves 5 units towards -Z
        cam.moveSpeed = 5.f;
        cam.onKey(Key::W, Action::Press);
        cam.update(1.f);
        CHECK_NEAR(cam.position.x, 0.f, 1e-6);
        CHECK_NEAR(cam.position.y, 0.f, 1e-6);
        CHECK_NEAR(cam.position.z, -5.f, 1e-5);
        cam.onKey(Key::W, Action::Release);

        // mouse look: moving mouse right turns right (yaw increases), pitch clamps
        cam.onMouseMove(1000.0, 0.0);
        CHECK(cam.yaw > 0.f);
        cam.onMouseMove(0.0, -1e7);
        CHECK(cam.pitch <= kPi * 0.5f);
        cam.onMouseMove(0.0, 1e7);
        CHECK(cam.pitch >= -kPi * 0.5f);
    }

    // --- free camera: roll via E, view = Rx(-p)Ry(-y)Rz(-r)T(-pos) ---
    {
        FreeCamera cam;
        cam.position = {1.f, 2.f, 3.f};
        cam.yaw = 0.3f;
        cam.pitch = -0.2f;
        cam.roll = 0.f;
        const Mat4 expected = rotateX(-cam.pitch) * rotateY(-cam.yaw) * rotateZ(-cam.roll) *
                              translate(-cam.position);
        CHECK(matNear(cam.view(), expected));

        cam.onKey(Key::E, Action::Press);
        cam.update(0.5f);
        cam.onKey(Key::E, Action::Release);
        CHECK_NEAR(cam.roll, 1.5f * 0.5f, 1e-6);

        // W moves along full 3D forward (not just ground plane)
        cam.roll = 0.f;
        cam.onKey(Key::W, Action::Press);
        const Vec3 before = cam.position;
        cam.update(0.1f);
        cam.onKey(Key::W, Action::Release);
        const Vec3 delta = cam.position - before;
        CHECK_NEAR(delta.y, std::sin(cam.pitch) * cam.moveSpeed * 0.1f, 1e-5);
    }

    // --- isometric: ortho projection + target maps to (0,0,-distance) in view space ---
    {
        IsometricCamera cam;
        cam.target = {1.f, 2.f, 3.f};
        cam.zoom = 2.f;
        cam.baseScale = 10.f;
        cam.distance = 20.f;

        const Mat4 proj = cam.projection(1.f); // square viewport
        CHECK_NEAR(proj(0, 0), 2.f / (2.f * (10.f / 2.f)), 1e-5); // 1/halfW, halfW = 5
        CHECK_NEAR(proj(1, 1), 0.2f, 1e-5);                        // 1/halfH = 1/5

        const Vec4 tv = cam.view() * Vec4(cam.target, 1.f);
        CHECK_NEAR(tv.x, 0.f, 1e-4);
        CHECK_NEAR(tv.y, 0.f, 1e-4);
        CHECK_NEAR(tv.z, -20.f, 1e-3);

        // scroll zooms in (zoom increases for positive dy)
        const float z0 = cam.zoom;
        cam.onScroll(0.0, 1.0);
        CHECK(cam.zoom > z0);
        // right-drag pans the target
        cam.onMouseButton(MouseButton::Right, Action::Press);
        const Vec3 t0 = cam.target;
        cam.onMouseMove(50.0, 0.0);
        CHECK(cam.target != t0);
        cam.onMouseButton(MouseButton::Right, Action::Release);
        // arrows pan via update()
        const Vec3 t1 = cam.target;
        cam.onKey(Key::Right, Action::Press);
        cam.update(0.5f);
        cam.onKey(Key::Right, Action::Release);
        CHECK(cam.target != t1);
    }
}
