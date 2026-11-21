// Unit tests for F34 GPU culling math: math::transformSphere (object -> world bounding
// sphere) and a CPU mirror of the compute cull (Frustum::containsSphere on the
// transformed sphere), which must agree with the expected visibility.

#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/cam.hpp>
#include <fuhgl/math.hpp>

#include <cmath>
#include <vector>

using namespace fuhgl;

void runCullingTests() {
    // --- transformSphere: identity keeps the sphere --------------------------------
    {
        const math::Vec4 s{1.f, 2.f, 3.f, 0.5f};
        const math::Vec4 w = math::transformSphere(math::Mat4{}, s);
        CHECK_NEAR(w.x, 1.f, 1e-6f);
        CHECK_NEAR(w.y, 2.f, 1e-6f);
        CHECK_NEAR(w.z, 3.f, 1e-6f);
        CHECK_NEAR(w.w, 0.5f, 1e-6f);
    }
    // --- translation moves the center, keeps the radius ----------------------------
    {
        const math::Mat4 t = math::translate({10.f, -5.f, 2.f});
        const math::Vec4 w = math::transformSphere(t, {1.f, 1.f, 1.f, 2.f});
        CHECK_NEAR(w.x, 11.f, 1e-5f);
        CHECK_NEAR(w.y, -4.f, 1e-5f);
        CHECK_NEAR(w.z, 3.f, 1e-5f);
        CHECK_NEAR(w.w, 2.f, 1e-5f);
    }
    // --- uniform scale scales the radius --------------------------------------------
    {
        const math::Mat4 s = math::scale({2.f, 2.f, 2.f});
        const math::Vec4 w = math::transformSphere(s, {0.f, 0.f, 0.f, 1.5f});
        CHECK_NEAR(w.w, 3.f, 1e-5f);
    }
    // --- non-uniform scale uses the largest axis (conservative) ---------------------
    {
        const math::Mat4 s = math::scale({2.f, 5.f, 3.f});
        const math::Vec4 w = math::transformSphere(s, {0.f, 0.f, 0.f, 1.f});
        CHECK_NEAR(w.w, 5.f, 1e-5f);
    }
    // --- rotation rotates the center, keeps the radius ------------------------------
    {
        const math::Mat4 r = math::rotateZ(math::radians(90.f)); // +X -> +Y
        const math::Vec4 w = math::transformSphere(r, {1.f, 0.f, 0.f, 0.25f});
        CHECK_NEAR(w.x, 0.f, 1e-5f);
        CHECK_NEAR(w.y, 1.f, 1e-5f);
        CHECK_NEAR(w.z, 0.f, 1e-5f);
        CHECK_NEAR(w.w, 0.25f, 1e-5f);
    }

    // --- CPU mirror of the compute cull: frustum vs transformed spheres --------------
    {
        FirstPersonCamera cam; // yaw=0 -> looks down -Z
        cam.position = {0.f, 0.f, 0.f};
        const math::Frustum f = math::Frustum::fromViewProj(cam.viewProj(1.f));
        const math::Vec4 localSphere{0.f, 0.f, 0.f, 0.5f};
        std::vector<math::Mat4> models = {
            math::translate({0.f, 0.f, -5.f}),  // in front -> visible
            math::translate({0.5f, 0.f, -3.f}), // in front -> visible
            math::translate({0.f, 0.f, 5.f}),   // behind the camera -> culled
            math::translate({100.f, 0.f, -5.f}) // far outside left -> culled
        };
        int visible = 0;
        for (const auto& m : models) {
            const math::Vec4 w = math::transformSphere(m, localSphere);
            if (f.containsSphere({w.x, w.y, w.z}, w.w)) ++visible;
        }
        CHECK(visible == 2);
    }
}
