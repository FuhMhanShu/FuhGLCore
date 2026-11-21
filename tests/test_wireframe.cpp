#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/render/wireframe.hpp>

#include <set>

using namespace fuhgl::render;
using fuhgl::math::Vec3;
using fuhgl::math::Mat4;

void runWireframeTests() {
    // box corners: 8 corners, min first
    const auto c = boxCorners({-1.f, -2.f, -3.f}, {4.f, 5.f, 6.f});
    CHECK(c.size() == 8);
    CHECK(c[0].x == -1.f && c[0].y == -2.f && c[0].z == -3.f);
    CHECK(c[7].x == 4.f && c[7].y == 5.f && c[7].z == 6.f);

    // box edges: 12 unique edges, all indices in [0,8), no degenerate edges
    CHECK(kBoxEdgeIndices.size() == 12);
    std::set<std::pair<unsigned, unsigned>> es;
    for (const auto& e : kBoxEdgeIndices) {
        CHECK(e[0] < 8 && e[1] < 8);
        CHECK(e[0] != e[1]);
        es.insert({std::min(e[0], e[1]), std::max(e[0], e[1])});
    }
    CHECK(es.size() == 12); // all distinct

    // wireframeEdges: quad (2 triangles, 6 indices) -> 4 boundary + 1 diagonal = 5 edges
    const MeshData quad = shapes::quadData(1.f, {1.f, 1.f, 1.f});
    CHECK(quad.indices.size() == 6);
    CHECK(wireframeEdges(quad).size() == 5);
    // box (24 verts, 36 indices, per-face vertices): 6 faces x (4 boundary + 1 diagonal) = 30
    const MeshData box = shapes::boxData({1.f, 1.f, 1.f}, {1.f, 1.f, 1.f});
    CHECK(box.indices.size() == 36);
    CHECK(wireframeEdges(box).size() == 30);
    // non-indexed mesh: 2 disjoint triangles -> 6 edges
    MeshData tri;
    tri.vertices = std::vector<float>(6 * MeshData::kFloatsPerVertex, 0.f);
    CHECK(wireframeEdges(tri).size() == 6);

    // Wireframe queueing works without a GL context (CPU-side only until render)
    Wireframe wf;
    CHECK(wf.lineCount() == 0);
    wf.addLine({0, 0, 0}, {1, 1, 1}, {1, 1, 1});
    CHECK(wf.lineCount() == 1);
    wf.addBox({-1, -1, -1}, {1, 1, 1}, {1, 0, 0});
    CHECK(wf.lineCount() == 13);
    wf.addMesh(box, Mat4{}, {0, 1, 0});
    CHECK(wf.lineCount() == 43); // 1 + 12 + 30
    auto sphere = makeSphere(0.5f);
    wf.addSolid(*sphere, 8, Mat4{}, {0, 0, 1});
    CHECK(wf.lineCount() > 43); // marching-tetrahedra mesh adds edges
    wf.addBoundingBox(*sphere, Mat4{}, {1, 1, 0});
    const std::size_t before = wf.lineCount();
    wf.addBoundingBox(*sphere, Mat4{}, {1, 1, 0});
    CHECK(wf.lineCount() == before + 12);
    wf.clear();
    CHECK(wf.lineCount() == 0);
    CHECK(!wf.isCreated()); // no GL context in unit tests
}
