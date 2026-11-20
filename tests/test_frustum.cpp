#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/cam.hpp>
#include <fuhgl/math.hpp>

using namespace fuhgl;
using namespace fuhgl::math;

void runFrustumTests() {
    // Camera at origin looking down -Z, fov 90, aspect 1, near 1, far 100.
    FirstPersonCamera cam;
    cam.position = {0.f, 0.f, 0.f};
    cam.yaw = 0.f;
    cam.pitch = 0.f;
    cam.fovY = kPi * 0.5f;
    cam.nearPlane = 1.f;
    cam.farPlane = 100.f;

    const Mat4 vp = cam.viewProj(1.f);
    const Frustum fr = Frustum::fromViewProj(vp);

    CHECK(fr.containsPoint({0.f, 0.f, -5.f}));    // in front, centered
    CHECK(!fr.containsPoint({0.f, 0.f, 5.f}));    // behind
    CHECK(!fr.containsPoint({0.f, 0.f, -0.5f}));  // closer than near
    CHECK(!fr.containsPoint({0.f, 0.f, -200.f})); // beyond far
    CHECK(!fr.containsPoint({100.f, 0.f, -5.f})); // far outside right
    CHECK(fr.containsSphere({0.f, 0.f, -5.f}, 1.f));
    CHECK(!fr.containsSphere({100.f, 0.f, -5.f}, 1.f));

    // Center pixel ray: origin on the near plane (z = -near), direction -Z.
    const Ray r = rayFromScreen(vp, 400.f, 300.f, 800.f, 600.f);
    CHECK_NEAR(r.origin.x, 0.f, 1e-4);
    CHECK_NEAR(r.origin.y, 0.f, 1e-4);
    CHECK_NEAR(r.origin.z, -1.f, 1e-4);
    CHECK_NEAR(r.dir.x, 0.f, 1e-4);
    CHECK_NEAR(r.dir.y, 0.f, 1e-4);
    CHECK_NEAR(r.dir.z, -1.f, 1e-4);

    // Corner pixel ray should point away from center axis.
    const Ray rc = rayFromScreen(vp, 0.f, 0.f, 800.f, 600.f);
    CHECK(rc.dir.x < -0.5f);
    CHECK(rc.dir.y > 0.5f);
    CHECK(rc.dir.z < 0.f);
}
