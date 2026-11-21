#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/math.hpp>
#include <fuhgl/render/graph.hpp>

#include <algorithm>
#include <cmath>

using namespace fuhgl;
using namespace fuhgl::render;

void runGraphTests() {
    // --- resolvePassOrder: linear chain a -> b -> c --------------------------------
    {
        std::vector<GraphPassDef> passes = {
            {"a", {}, {"x"}},
            {"b", {"x"}, {"y"}},
            {"c", {"y"}, {"z"}},
        };
        std::vector<int> order;
        CHECK(resolvePassOrder(passes, order));
        CHECK(order.size() == 3);
        const auto pos = [&](int p) { return std::find(order.begin(), order.end(), p) - order.begin(); };
        CHECK(pos(0) < pos(1) && pos(1) < pos(2));
    }
    // --- diamond: d depends on b and c ----------------------------------------------
    {
        std::vector<GraphPassDef> passes = {
            {"a", {}, {"x"}},
            {"b", {"x"}, {"y"}},
            {"c", {"x"}, {"z"}},
            {"d", {"y", "z"}, {"w"}},
        };
        std::vector<int> order;
        CHECK(resolvePassOrder(passes, order));
        const auto pos = [&](int p) { return std::find(order.begin(), order.end(), p) - order.begin(); };
        CHECK(pos(0) < pos(1) && pos(0) < pos(2) && pos(1) < pos(3) && pos(2) < pos(3));
    }
    // --- cycle: a <-> b ---------------------------------------------------------------
    {
        std::vector<GraphPassDef> passes = {
            {"a", {"y"}, {"x"}},
            {"b", {"x"}, {"y"}},
        };
        std::vector<int> order;
        CHECK(!resolvePassOrder(passes, order));
    }
    // --- feedback pass (TAA-style): reads+writes its own output, no false cycle --------
    {
        std::vector<GraphPassDef> passes = {
            {"light", {}, {"hdr"}},
            {"taa", {"hdr", "taa"}, {"taa"}, /*feedback=*/true},
            {"out", {"taa"}, {}},
        };
        std::vector<int> order;
        CHECK(resolvePassOrder(passes, order));
        const auto pos = [&](int p) { return std::find(order.begin(), order.end(), p) - order.begin(); };
        CHECK(pos(0) < pos(1) && pos(1) < pos(2));
    }
    // --- ping-pong: multiple writers of one resource are ordered by declaration -------
    {
        std::vector<GraphPassDef> passes = {
            {"bright", {}, {"a"}},
            {"h", {"a"}, {"b"}},
            {"v", {"b"}, {"a"}},
            {"done", {"a"}, {}},
        };
        std::vector<int> order;
        CHECK(resolvePassOrder(passes, order));
        const auto pos = [&](int p) { return std::find(order.begin(), order.end(), p) - order.begin(); };
        CHECK(pos(0) < pos(1) && pos(1) < pos(2) && pos(2) < pos(3));
    }

    // --- math: halton + jitterViewProj (F32 helpers) ----------------------------------
    const math::Vec2 h0 = math::halton(0);
    CHECK_NEAR(h0.x, 0.f, 1e-6f);
    CHECK_NEAR(h0.y, 0.f, 1e-6f);
    const math::Vec2 h1 = math::halton(1);
    CHECK_NEAR(h1.x, 0.5f, 1e-6f);       // base 2: 0.1b = 0.5
    CHECK_NEAR(h1.y, 1.f / 3.f, 1e-6f);   // base 3: 0.1b = 1/3
    for (std::uint32_t i = 0; i < 100; ++i) {
        const math::Vec2 h = math::halton(i);
        CHECK(h.x >= 0.f && h.x < 1.f && h.y >= 0.f && h.y < 1.f);
    }
    CHECK_NEAR(math::radicalInverse(1), 0.5f, 1e-7f);
    // jitterViewProj: a jitter of (0.1, 0) shifts NDC x by +0.1 for any point
    {
        const math::Mat4 vp = math::perspective(math::radians(60.f), 1.f, 0.1f, 100.f) *
                              math::lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
        const math::Vec3 p{1.f, 0.5f, 0.f};
        const math::Vec4 c0 = vp * math::Vec4(p, 1.f);
        const math::Vec4 c1 = math::jitterViewProj(vp, {0.1f, 0.f}) * math::Vec4(p, 1.f);
        CHECK_NEAR(c1.x / c1.w - c0.x / c0.w, 0.1f, 1e-5f);
        CHECK_NEAR(c1.y / c1.w - c0.y / c0.w, 0.f, 1e-5f);
    }
}
