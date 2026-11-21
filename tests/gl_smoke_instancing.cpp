// Headless smoke test for instanced rendering (F24): one box mesh drawn with
// 3 per-instance model matrices through DeferredPipeline must light three probe
// pixels; after updateInstanceData() (all instances moved left) the center probe
// must go sky; after clearInstanceData() a single non-instanced draw remains.
// Verifies Mesh::setInstanceData/updateInstanceData/clearInstanceData/instanceCount
// and the uInstanced shader path; checks for GL errors. Skips without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

using Px = std::array<unsigned char, 4>;

bool isLit(const Px& px) {
    return px[3] == 255 && px[0] > px[2] && (px[0] + px[1] + px[2]) > 60;
}

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_instancing", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_instancing: no GL context — skipping\n");
        return 0;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_instancing: context below GL 3.3 — skipping\n");
        return 0;
    }

    const int W = 256, H = 256;
    fuhgl::render::DeferredPipeline pipeline;
    if (!pipeline.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_instancing: pipeline create failed\n");
        return 1;
    }

    fuhgl::render::Mesh boxMesh;
    fuhgl::render::shapes::box(boxMesh, {0.45f, 0.45f, 0.45f}, {1.f, 1.f, 1.f});
    fuhgl::render::Material boxMat = fuhgl::render::materials::matte({0.95f, 0.55f, 0.2f});

    // three instances spread along X
    std::vector<fuhgl::math::Mat4> models = {
        fuhgl::math::translate({-1.2f, 0.f, 0.f}),
        fuhgl::math::Mat4{},
        fuhgl::math::translate({1.2f, 0.f, 0.f}),
    };
    boxMesh.setInstanceData(models.data(), static_cast<fuhgl::gl::GLsizei>(models.size()));
    if (boxMesh.instanceCount() != 3) {
        std::fprintf(stderr, "gl_smoke_instancing: FAIL — instanceCount=%d\n", boxMesh.instanceCount());
        return 1;
    }

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 0.9f, 4.4f};
    cam.pitch = -0.12f;

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.items.push_back({&boxMesh, &boxMat, fuhgl::math::Mat4{}});
    scene.lights.push_back({{3.f, 4.f, 5.f}, {1.f, 0.95f, 0.85f}, 25.f});

    const int py = 140;
    const int pxL = W / 2 - 58, pxC = W / 2, pxR = W / 2 + 58;
    auto renderAndProbe = [&](int x, int y) {
        pipeline.render(scene);
        fuhgl::gl::flush();
        pipeline.output().bind();
        Px px{};
        fuhgl::gl::readPixels(x, y, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        return px;
    };

    // --- 1) instanced draw: three boxes visible --------------------------------------
    const Px pl = renderAndProbe(pxL, py);
    const Px pc = renderAndProbe(pxC, py);
    const Px pr = renderAndProbe(pxR, py);
    const Px corner = renderAndProbe(2, 2);
    if (fuhgl::gl::lastError() != fuhgl::gl::Error::NoError) {
        std::fprintf(stderr, "gl_smoke_instancing: FAIL — GL error after instanced render\n");
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_instancing: left=(%d,%d,%d) center=(%d,%d,%d) right=(%d,%d,%d)\n",
                 pl[0], pl[1], pl[2], pc[0], pc[1], pc[2], pr[0], pr[1], pr[2]);
    if (!isLit(pl) || !isLit(pc) || !isLit(pr)) {
        std::fprintf(stderr, "gl_smoke_instancing: FAIL — instanced boxes not all visible\n");
        return 1;
    }
    if (!(corner[2] > corner[0])) {
        std::fprintf(stderr, "gl_smoke_instancing: FAIL — corner is not sky\n");
        return 1;
    }

    // --- 2) updateInstanceData: move all three instances onto the left box -----------
    const std::vector<fuhgl::math::Mat4> moved(3, fuhgl::math::translate({-1.2f, 0.f, 0.f}));
    boxMesh.updateInstanceData(moved.data(), 3);
    const Px c2 = renderAndProbe(pxC, py);
    const Px l2 = renderAndProbe(pxL, py);
    if (fuhgl::gl::lastError() != fuhgl::gl::Error::NoError) {
        std::fprintf(stderr, "gl_smoke_instancing: FAIL — GL error after update\n");
        return 1;
    }
    if (isLit(c2) || !isLit(l2)) {
        std::fprintf(stderr, "gl_smoke_instancing: FAIL — updateInstanceData had no effect (center=(%d,%d,%d) left=(%d,%d,%d))\n",
                     c2[0], c2[1], c2[2], l2[0], l2[1], l2[2]);
        return 1;
    }

    // --- 3) clearInstanceData: back to one single draw at the origin -----------------
    boxMesh.clearInstanceData();
    if (boxMesh.instanceCount() != 0) {
        std::fprintf(stderr, "gl_smoke_instancing: FAIL — clearInstanceData\n");
        return 1;
    }
    const Px c3 = renderAndProbe(pxC, py);
    const Px l3 = renderAndProbe(pxL, py);
    if (!isLit(c3) || isLit(l3)) {
        std::fprintf(stderr, "gl_smoke_instancing: FAIL — single draw wrong (center=(%d,%d,%d) left=(%d,%d,%d))\n",
                     c3[0], c3[1], c3[2], l3[0], l3[1], l3[2]);
        return 1;
    }

    std::fprintf(stderr, "gl_smoke_instancing: OK\n");
    return 0;
}
