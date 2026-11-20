// Headless deferred-pipeline smoke test: renders one lit box through
// DeferredPipeline into its output FBO and verifies the center pixel is lit
// and a corner pixel shows the sky gradient. Skips cleanly without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <cstdio>
#include <cstdlib>
#include <vector>

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_deferred", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_deferred: no GL context — skipping\n");
        return 0;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_deferred: context below GL 3.3 — skipping\n");
        return 0;
    }

    fuhgl::render::DeferredPipeline pipeline;
    if (!pipeline.create(64, 64)) {
        std::fprintf(stderr, "gl_smoke_deferred: pipeline create failed\n");
        return 1;
    }

    fuhgl::render::Mesh box;
    fuhgl::render::shapes::box(box, {1.f, 1.f, 1.f}, {0.9f, 0.3f, 0.2f});

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 0.f, 4.f};

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = 64;
    scene.height = 64;
    scene.aspect = 1.f;
    scene.items.push_back({&box, nullptr, fuhgl::math::Mat4{}});
    scene.lights.push_back({{2.f, 2.f, 3.f}, {1.f, 1.f, 1.f}, 20.f});

    pipeline.render(scene);
    fuhgl::gl::flush();

    // cross-check with the forward pipeline in the same scene
    fuhgl::render::ForwardPipeline fwd;
    if (fwd.create()) {
        fuhgl::Framebuffer fwdTarget;
        fuhgl::Texture fwdTex(fuhgl::Texture::Target::Tex2D);
        fuhgl::Renderbuffer fwdDepth;
        fwdTarget.create();
        fwdTarget.setSize(64, 64);
        fwdTex.bind();
        fwdTex.texStorage2D(1, fuhgl::gl::GL_RGBA8, 64, 64);
        fwdDepth.create();
        fwdDepth.bind();
        fwdDepth.storage(fuhgl::gl::GL_DEPTH_COMPONENT24, 64, 64);
        fwdTarget.bind();
        fwdTarget.attachTexture2D(0, fwdTex);
        fwdTarget.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, fwdDepth);
        fwd.setTarget(&fwdTarget);
        fwd.render(scene);
        fuhgl::gl::flush();
        fwdTarget.bind();
        unsigned char fcenter[4] = {0, 0, 0, 0};
        fuhgl::gl::readPixels(32, 32, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, fcenter);
        std::fprintf(stderr, "gl_smoke_deferred[debug]: forward center=(%d,%d,%d,%d)\n",
                     fcenter[0], fcenter[1], fcenter[2], fcenter[3]);
        fuhgl::Framebuffer::unbind();
    }

    if (std::getenv("FUHGL_DEBUG")) {
        // dump the first bytes of the light UBO as seen by the GPU
        auto& f = fuhgl::gl::functions();
        f.glBindBuffer(fuhgl::gl::GL_UNIFORM_BUFFER, pipeline.lightsUbo().id());
        if (void* p = f.glMapBuffer(fuhgl::gl::GL_UNIFORM_BUFFER, fuhgl::gl::GL_READ_ONLY)) {
            const float* v = static_cast<const float*>(p);
            std::fprintf(stderr, "gl_smoke_deferred[debug]: UBO[0..3] = %f %f %f %f, UBO[64..67] = %f %f %f %f\n",
                         v[0], v[1], v[2], v[3], v[256], v[257], v[258], v[259]);
            f.glUnmapBuffer(fuhgl::gl::GL_UNIFORM_BUFFER);
        }
    }

    pipeline.output().bind();
    unsigned char center[4] = {0, 0, 0, 0}, corner[4] = {0, 0, 0, 0};
    fuhgl::gl::readPixels(32, 32, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, center);
    fuhgl::gl::readPixels(2, 2, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, corner);
    std::fprintf(stderr, "gl_smoke_deferred: center=(%d,%d,%d,%d) corner=(%d,%d,%d,%d)\n",
                 center[0], center[1], center[2], center[3], corner[0], corner[1], corner[2], corner[3]);

    // center should be the lit box (reddish, clearly not the sky), corner the sky (bluish)
    const bool centerLit = center[0] > 40 && center[3] == 255;
    const bool cornerSky = corner[2] > corner[0]; // sky is blue-dominant
    if (!centerLit || !cornerSky) {
        std::fprintf(stderr, "gl_smoke_deferred: FAIL\n");
        return 1;
    }
    return 0;
}
