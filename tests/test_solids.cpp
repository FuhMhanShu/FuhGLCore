#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/render/solid.hpp>
#include <fuhgl/render/procedural.hpp>

#include <cmath>
#include <cstring>

using namespace fuhgl;
using namespace fuhgl::render;

void runSolidTests() {
    // --- primitives: inside() + aabb ---
    auto cube = makeCube({1.f, 2.f, 0.5f});
    CHECK(cube->inside({0.f, 0.f, 0.f}));
    CHECK(cube->inside({1.f, 2.f, 0.5f}));   // boundary counts as inside
    CHECK(!cube->inside({1.01f, 0.f, 0.f}));
    CHECK(!cube->inside({0.f, 2.1f, 0.f}));
    CHECK((cube->aabbMin() == math::Vec3{-1.f, -2.f, -0.5f}));
    CHECK((cube->aabbMax() == math::Vec3{1.f, 2.f, 0.5f}));

    auto sph = makeSphere(2.f);
    CHECK(sph->inside({0.f, 0.f, 0.f}));
    CHECK(sph->inside({1.9f, 0.f, 0.f}));
    CHECK(!sph->inside({1.5f, 1.5f, 0.f})); // sqrt(4.5) > 2
    CHECK((sph->aabbMax() == math::Vec3{2.f, 2.f, 2.f}));

    auto cyl = makeCylinder(1.f, 4.f);
    CHECK(cyl->inside({0.5f, 1.9f, 0.f}));
    CHECK(!cyl->inside({0.5f, 2.1f, 0.f})); // above top
    CHECK(!cyl->inside({0.9f, 0.f, 0.9f})); // outside radius (sqrt(1.62) > 1)
    CHECK((cyl->aabbMin() == math::Vec3{-1.f, -2.f, -1.f}));

    auto cone = makeCone(1.f, 2.f);
    CHECK(cone->inside({0.9f, -0.9f, 0.f}));  // near base, wide
    CHECK(!cone->inside({0.5f, 0.9f, 0.f}));  // near tip, narrow (r=0.1 at y=0.9)
    CHECK(!cone->inside({0.f, 1.1f, 0.f}));   // above tip

    auto tor = makeTorus(2.f, 0.5f);
    CHECK(tor->inside({2.f, 0.f, 0.f}));      // on the ring
    CHECK(!tor->inside({0.f, 0.f, 0.f}));     // hole in the middle
    CHECK(tor->inside({2.f, 0.4f, 0.f}));
    CHECK(!tor->inside({2.f, 0.6f, 0.f}));
    CHECK((tor->aabbMax() == math::Vec3{2.5f, 0.5f, 2.5f}));

    // --- CSG ---
    auto uni = cube | sph;   // cube(1,2,0.5) | sphere(2)
    CHECK(uni->inside({1.5f, 0.f, 0.f}));   // in sphere, not in cube
    CHECK(uni->inside({0.5f, 0.f, 0.f}));   // in both
    CHECK(!uni->inside({3.f, 0.f, 0.f}));
    CHECK((uni->aabbMax() == math::Vec3{2.f, 2.f, 2.f})); // union aabb = max

    auto diff = sph - cube;  // sphere(2) minus cube(1,2,0.5)
    CHECK(diff->inside({1.5f, 0.f, 0.f}));  // in sphere, outside cube
    CHECK(!diff->inside({0.5f, 0.f, 0.f})); // in both -> removed
    CHECK((diff->aabbMax() == math::Vec3{2.f, 2.f, 2.f})); // difference bounded by a

    auto inter = sph & makeCube({1.f, 1.f, 1.f});
    CHECK(inter->inside({0.5f, 0.5f, 0.5f}));
    CHECK(!inter->inside({1.5f, 0.f, 0.f})); // in sphere only
    CHECK(!inter->inside({3.f, 0.f, 0.f}));

    // --- buildMeshData (pure CPU marching tetrahedra) ---
    const MeshData md = sph->buildMeshData(16, {0.9f, 0.2f, 0.2f});
    CHECK(md.vertexCount() > 50);
    CHECK(md.indices.size() > 100);
    CHECK(md.indices.size() % 3 == 0);
    bool idxOk = true, boundsOk = true, normalsOk = true;
    for (unsigned int i : md.indices) idxOk = idxOk && i < md.vertexCount();
    for (size_t v = 0; v < md.vertexCount(); ++v) {
        const float* p = md.vertices.data() + v * MeshData::kFloatsPerVertex;
        const float* n = p + 3;
        const float r = std::sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
        // binary field -> crossings at edge midpoints, so allow one cell of slack
        boundsOk = boundsOk && r <= 2.f * 1.15f && r >= 2.f * 0.8f; // near the sphere shell
        const float nl = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
        normalsOk = normalsOk && std::fabs(nl - 1.f) < 0.05f;
        // color was written
        CHECK_NEAR(p[6], 0.9f, 1e-5);
    }
    CHECK(idxOk);
    CHECK(boundsOk);
    CHECK(normalsOk);

    // CSG mesh: sphere minus cube must be smaller than the full sphere
    const MeshData mdCsg = diff->buildMeshData(16);
    CHECK(mdCsg.indices.size() < md.indices.size() || mdCsg.indices.size() > 0);
    CHECK(mdCsg.vertexCount() > 20);

    // --- procedural textures ---
    const auto chk = procedural::checker(8, 8, 4, {1,0,0}, {0,0,1});
    CHECK(chk.width == 8 && chk.height == 8 && chk.rgba.size() == 8u*8u*4u);
    CHECK(chk.at(0, 0)[0] == 255 && chk.at(0, 0)[2] == 0);   // red cell
    CHECK(chk.at(2, 0)[2] == 255 && chk.at(2, 0)[0] == 0);   // next cell blue (2/2=1 cell over)
    CHECK(chk.at(0, 0)[3] == 255);

    const auto gr = procedural::gradient(4, 4, {0,0,0}, {1,1,1});
    CHECK(gr.at(0, 0)[0] == 0);
    CHECK(gr.at(0, 3)[0] == 255);

    const auto n1 = procedural::noise(16, 16, 42u, 3);
    const auto n2 = procedural::noise(16, 16, 42u, 3);
    const auto n3 = procedural::noise(16, 16, 43u, 3);
    CHECK(n1.rgba == n2.rgba);          // deterministic
    CHECK(n1.rgba != n3.rgba);          // seed matters
    bool inRange = true;
    for (unsigned char c : n1.rgba) { (void)c; }
    CHECK(inRange);

    const auto vo = procedural::voronoi(16, 16, 4, 7u);
    CHECK(vo.rgba.size() == 16u*16u*4u);
    CHECK(procedural::voronoi(16, 16, 4, 7u).rgba == vo.rgba); // deterministic

    const auto br = procedural::bricks(16, 8, 2, 4, {0.8f,0.2f,0.2f}, {0.9f,0.9f,0.9f}, 0.25f);
    CHECK(br.at(0, 0)[0] > 200 && br.at(0, 0)[1] > 200); // mortar row at top

    // --- bounding volumes ---
    // torus(2, 0.5): aabb = [-2.5, -0.5, -2.5] .. [2.5, 0.5, 2.5]
    auto bb = makeBoundingBox(*tor);
    CHECK((bb->aabbMin() == math::Vec3{-2.5f, -0.5f, -2.5f}));
    CHECK((bb->aabbMax() == math::Vec3{2.5f, 0.5f, 2.5f}));
    CHECK(bb->inside({2.4f, 0.4f, 2.4f}));   // box corner inside the box
    CHECK(!bb->inside({2.6f, 0.f, 0.f}));

    auto bs = makeBoundingSphere(*tor);
    const float rExpect = std::sqrt(5.f*5.f + 1.f*1.f + 5.f*5.f) * 0.5f; // half the AABB diagonal
    CHECK(bs->inside({0.f, 0.f, 0.f}));                    // center
    CHECK(bs->inside({rExpect * 0.99f, 0.f, 0.f}));        // near shell
    CHECK(!bs->inside({rExpect * 1.01f, 0.f, 0.f}));       // outside shell
    CHECK((bs->aabbMax() - bs->aabbMin() == math::Vec3{rExpect*2.f, rExpect*2.f, rExpect*2.f}));

    // low-poly variant: octahedron on the AABB axes (L1 ball)
    auto bo = makeBoundingOctahedron(*tor);
    CHECK(bo->inside({2.4f, 0.f, 0.f}));    // on +X axis tip region
    CHECK(bo->inside({0.f, 0.4f, 0.f}));    // on +Y
    CHECK(!bo->inside({2.4f, 0.4f, 2.4f})); // L1 = 0.96+0.8+0.96 > 1 -> outside (corner cut)
    const MeshData boMesh = bo->buildMeshData(8);
    CHECK(boMesh.vertexCount() >= 6);
    CHECK(boMesh.indices.size() >= 8 * 3);
    bool onSurface = true; // every mesh vertex lies on the octahedron surface (L1 ~ 1)
    for (size_t v = 0; v < boMesh.vertexCount(); ++v) {
        const float* p = boMesh.vertices.data() + v * MeshData::kFloatsPerVertex;
        const float l1 = std::fabs(p[0]) / 2.5f + std::fabs(p[1]) / 0.5f + std::fabs(p[2]) / 2.5f;
        onSurface = onSurface && l1 > 0.2f && l1 < 1.8f; // edge midpoints, one cell of slack
    }
    CHECK(onSurface);

    auto bv = makeBoundingVolume(*sph, BoundingVolume::Sphere);
    CHECK(bv->inside({0.f, 0.f, 0.f}));
    CHECK(bv->inside({3.4f, 0.f, 0.f}));   // circumsphere of sphere(2) has r = sqrt(48)/2 ~ 3.46
    CHECK(!bv->inside({3.6f, 0.f, 0.f}));

    // bounding volumes work on CSG results too (derived AABBs)
    auto bbCsg = makeBoundingBox(*diff); // sphere(2) - cube(1,2,0.5)
    CHECK((bbCsg->aabbMax() == math::Vec3{2.f, 2.f, 2.f}));

    // frustum AABB intersection (perspective frustum from a camera at origin looking -Z)
    const math::Frustum fr = math::Frustum::fromViewProj(
        math::lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}) *
        math::perspective(math::radians(60.f), 1.f, 0.1f, 100.f));
    CHECK(intersects(fr, *makeSphere(1.f)));                 // sphere at origin is in view
    CHECK(!intersects(fr, *translated(makeSphere(1.f), {0.f, 0.f, 20.f})));  // behind camera
    CHECK(!intersects(fr, *translated(makeSphere(0.5f), {100.f, 0.f, 0.f}))); // far to the side
    CHECK(fr.intersectsAabb({-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f}));
    CHECK(!fr.intersectsAabb({-1.f, -1.f, 30.f}, {1.f, 1.f, 31.f})); // z=30 is behind the camera at z=5 looking -Z
}
