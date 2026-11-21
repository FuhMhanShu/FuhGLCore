#include "test_framework.hpp"
#include "tests.hpp"

#include <cmath>

#include <fuhgl/math.hpp>
#include <fuhgl/render/mesh.hpp>

using namespace fuhgl;
using namespace fuhgl::render;

void runMeshTests() {
    CHECK(MeshData::kFloatsPerVertex == 11);
    CHECK(MeshData::kVertexBytes == 44);
    MeshData d;
    d.vertices.resize(3 * MeshData::kFloatsPerVertex, 0.f);
    CHECK(d.vertexCount() == 3);

    // shapes::boxData: every face's triangles must wind CCW around the outward normal,
    // otherwise the face is back-facing (culled) when viewed from outside.
    const math::Vec3 h{1.f, 2.f, 3.f};
    const MeshData b = shapes::boxData(h, {1.f, 1.f, 1.f});
    CHECK(b.vertexCount() == 24);
    CHECK(b.indices.size() == 36);
    for (int f = 0; f < 6; ++f) {
        const unsigned int base = static_cast<unsigned int>(f) * 4;
        const float* v0 = b.vertices.data() + static_cast<size_t>(base) * MeshData::kFloatsPerVertex;
        // face normal = stored normal of vertex 0
        const math::Vec3 n{v0[3], v0[4], v0[5]};
        for (int t = 0; t < 2; ++t) {
            const unsigned int ia = b.indices[f * 6 + t * 3 + 0];
            const unsigned int ib = b.indices[f * 6 + t * 3 + 1];
            const unsigned int ic = b.indices[f * 6 + t * 3 + 2];
            const float* a = b.vertices.data() + static_cast<size_t>(ia) * MeshData::kFloatsPerVertex;
            const float* bb = b.vertices.data() + static_cast<size_t>(ib) * MeshData::kFloatsPerVertex;
            const float* c = b.vertices.data() + static_cast<size_t>(ic) * MeshData::kFloatsPerVertex;
            const math::Vec3 ab{bb[0] - a[0], bb[1] - a[1], bb[2] - a[2]};
            const math::Vec3 ac{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
            CHECK(math::dot(math::normalize(math::cross(ab, ac)), n) > 0.99f);
        }
        // all 4 corners lie on the face plane: dot(p, n) == |dot(h, n)| (face center = n*h componentwise)
        for (int i = 0; i < 4; ++i) {
            const float* p = b.vertices.data() + static_cast<size_t>(base + i) * MeshData::kFloatsPerVertex;
            CHECK_NEAR(math::dot(math::Vec3{p[0], p[1], p[2]}, n), std::fabs(math::dot(h, n)), 1e-4f);
        }
    }

    // shapes::quadData: XY plane quad, CCW from +Z
    const MeshData q = shapes::quadData(2.f, {0.5f, 0.5f, 0.5f});
    CHECK(q.vertexCount() == 4);
    CHECK(q.indices.size() == 6);
    {
        const float* a = q.vertices.data();
        const float* b = q.vertices.data() + MeshData::kFloatsPerVertex;
        const float* c = q.vertices.data() + 2 * MeshData::kFloatsPerVertex;
        const math::Vec3 ab{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const math::Vec3 ac{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        CHECK(math::cross(ab, ac).z > 0.f);
    }
}
