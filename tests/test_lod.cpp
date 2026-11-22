// Unit tests for F41 LOD: LodMesh level selection/clamping, defaultDistances heuristic,
// resolveLodMesh (with/without lodEnabled, with/without item.lod), create() validation.

#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/math.hpp>
#include <fuhgl/render/lod.hpp>
#include <fuhgl/render/mesh.hpp>

using namespace fuhgl;

void runLodTests() {
    using render::LodMesh;
    auto meshes = [](int n) {
        std::vector<render::Mesh> v;
        v.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) v.emplace_back();
        return v;
    };
    // create() validation: empty levels, mismatched distances, non-ascending distances
    {
        LodMesh m;
        CHECK(!m.create({}, {}));                           // empty levels invalid
        CHECK(!m.create(meshes(2), {}));                    // 2 levels need 1 distance
        CHECK(m.create(meshes(1), {}));                     // 1 level, 0 distances: degenerate but valid
        CHECK(m.create(meshes(2), {5.f}));                  // ok: 1 distance, 2 levels
        CHECK(!m.create(meshes(3), {10.f, 5.f}));           // not ascending
        CHECK(m.create(meshes(3), {5.f, 10.f}));
    }
    // selectLevel: clamping + thresholds
    {
        LodMesh m;
        CHECK(m.create(meshes(3), {5.f, 10.f}));
        CHECK(m.levelCount() == 3);
        CHECK(m.selectLevel(0.f) == 0);
        CHECK(m.selectLevel(4.999f) == 0);
        CHECK(m.selectLevel(5.f) == 1);
        CHECK(m.selectLevel(9.9f) == 1);
        CHECK(m.selectLevel(10.f) == 2);
        CHECK(m.selectLevel(1000.f) == 2);   // clamped to last
        CHECK(m.select(3.f) == &m.level(0));
        CHECK(m.select(7.f) == &m.level(1));
        CHECK(m.select(50.f) == &m.level(2));
        LodMesh empty;
        CHECK(empty.selectLevel(1.f) == -1);
        CHECK(empty.select(1.f) == nullptr);
    }
    // defaultDistances: monotonic, matches the screen-fraction heuristic
    {
        const float fov = math::radians(60.f);
        auto d = LodMesh::defaultDistances(fov, 2.f, {0.5f, 0.25f});
        CHECK(d.size() == 2);
        CHECK(d[0] < d[1]);
        // fraction 0.5: d = r / (f * tan(fov/2)) = 2 / (0.5 * tan(30deg))
        CHECK_NEAR(d[0], 2.f / (0.5f * std::tan(fov * 0.5f)), 1e-4f);
        CHECK_NEAR(d[1], 2.f / (0.25f * std::tan(fov * 0.5f)), 1e-4f);
    }
    // resolveLodMesh: no lod -> item.mesh; lod + enabled -> level by distance; disabled -> item.mesh
    {
        LodMesh m;
        CHECK(m.create(meshes(2), {5.f}));
        render::Mesh base;
        render::DrawItem item;
        item.mesh = &base;
        int level = -2;
        // no lod set -> base mesh, level -1
        CHECK(render::resolveLodMesh(item, {0.f, 0.f, 0.f}, true, &level) == &base);
        CHECK(level == -1);
        // lod set, camera at origin, item at origin (dist 0) -> level 0
        item.lod = &m;
        item.model = math::Mat4{};
        CHECK(render::resolveLodMesh(item, {0.f, 0.f, 0.f}, true, &level) == &m.level(0));
        CHECK(level == 0);
        // camera 100 away -> level 1
        CHECK(render::resolveLodMesh(item, {0.f, 0.f, 100.f}, true, &level) == &m.level(1));
        CHECK(level == 1);
        // lodEnabled = false -> base mesh even with lod set
        CHECK(render::resolveLodMesh(item, {0.f, 0.f, 100.f}, false, &level) == &base);
        CHECK(level == -1);
        // model translation moves the item: translate (0,0,100), camera at origin -> dist 100 -> level 1
        item.model = math::translate({0.f, 0.f, 100.f});
        CHECK(render::resolveLodMesh(item, {0.f, 0.f, 0.f}, true, &level) == &m.level(1));
        CHECK(level == 1);
    }
}
