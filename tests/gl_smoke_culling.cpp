// Headless smoke test for F34 GpuCuller: 1000 box instances (500 in front of the camera,
// 500 behind) are culled by the compute shader into an indirect command; the visible
// count must be read back from the GPU counter, the indirect draw must light pixels,
// and a camera looking away must cull everything (0 visible, 0 lit pixels).
// Skips cleanly without GL >= 4.3 (compute shaders).

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int g_failures = 0;
void expect(bool cond, const char* what) {
    std::fprintf(stderr, "  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) ++g_failures;
}

constexpr const char* kVs = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 4) in mat4 aInstanceModel;
uniform mat4 uViewProj;
out vec3 vColor;
void main() {
    gl_Position = uViewProj * (aInstanceModel * vec4(aPos, 1.0));
    vColor = vec3(1.0, 0.6, 0.2);
}
)GLSL";

constexpr const char* kFs = R"GLSL(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)GLSL";

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_culling", 128, 128)) {
        std::fprintf(stderr, "gl_smoke_culling: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(4, 3)) {
        std::fprintf(stderr, "gl_smoke_culling: context below GL 4.3 — SKIP\n");
        return 77;
    }

    const int W = 128, H = 128;

    fuhgl::render::Mesh boxMesh;
    fuhgl::render::shapes::box(boxMesh, {0.3f, 0.3f, 0.3f}, {1.f, 1.f, 1.f});
    expect(boxMesh.boundingSphere().w > 0.4f && boxMesh.boundingSphere().w < 0.6f,
           "mesh bounding sphere of a 0.6 box has radius ~0.52");

    fuhgl::render::GpuCuller culler;
    if (!culler.create(1024)) {
        std::fprintf(stderr, "gl_smoke_culling: culler create failed\n");
        return 1;
    }
    culler.setMesh(boxMesh);

    // 500 instances in front (z < 0), 500 behind the camera (z > 0)
    std::vector<fuhgl::math::Mat4> models;
    models.reserve(1000);
    for (int i = 0; i < 500; ++i) {
        const float x = -2.f + 4.f * (i % 25) / 24.f;
        const float y = -1.f + 2.f * ((i / 25) % 10) / 9.f;
        const float z = -2.f - 4.f * (i / 250);
        models.push_back(fuhgl::math::translate({x, y, z}));
    }
    for (int i = 0; i < 500; ++i)
        models.push_back(fuhgl::math::translate({0.f, 0.f, 2.f + 4.f * i / 500.f}));
    culler.setInstances(models.data(), static_cast<int>(models.size()));
    expect(culler.instanceCount() == 1000, "1000 instances uploaded");

    fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
    if (!vs.compile(kVs) || !fs.compile(kFs)) {
        std::fprintf(stderr, "gl_smoke_culling: shader compile failed\n");
        return 1;
    }
    fuhgl::Program prog;
    prog.create();
    prog.attach(vs);
    prog.attach(fs);
    if (!prog.link()) {
        std::fprintf(stderr, "gl_smoke_culling: link failed: %s\n", prog.infoLog().c_str());
        return 1;
    }

    fuhgl::Texture colorTex(fuhgl::Texture::Target::Tex2D);
    colorTex.bind();
    colorTex.setImage2D(0, fuhgl::gl::GL_RGBA8, W, H, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, nullptr);
    colorTex.setParameteri(fuhgl::gl::GL_TEXTURE_MIN_FILTER, fuhgl::gl::GL_NEAREST);
    colorTex.setParameteri(fuhgl::gl::GL_TEXTURE_MAG_FILTER, fuhgl::gl::GL_NEAREST);
    fuhgl::Framebuffer fbo;
    fbo.create();
    fbo.setSize(W, H);
    fbo.bind();
    fbo.attachTexture2D(0, colorTex);
    expect(fbo.isComplete(), "FBO complete");
    fuhgl::gl::viewport(0, 0, W, H); // surfaceless contexts start with a 0x0 viewport

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 0.f, 0.f};
    cam.yaw = 0.f; // look down -Z: the front grid is visible, the back grid is not

    auto renderAndCount = [&]() {
        culler.cull(cam.viewProj(1.f));
        const int visible = culler.lastVisibleCount();
        unsigned int cmdInstanceCount = 0;
        culler.commandBuffer().getSubData(4, 4, &cmdInstanceCount);
        fbo.bind();
        const float clear[4] = {0.05f, 0.05f, 0.09f, 1.f};
        fuhgl::gl::clearBufferfv(fuhgl::gl::GL_COLOR, 0, clear);
        fuhgl::gl::disable(fuhgl::gl::Cap::CullFace);
        fuhgl::gl::disable(fuhgl::gl::Cap::DepthTest);
        prog.use();
        prog.setUniform("uViewProj", cam.viewProj(1.f));
        culler.draw();
        fuhgl::gl::flush();
        std::vector<unsigned char> px(static_cast<size_t>(W) * H * 4, 0);
        fuhgl::gl::readPixels(0, 0, W, H, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        int lit = 0;
        for (size_t i = 0; i < px.size(); i += 4)
            if (px[i] > 100) ++lit;
        return std::make_tuple(visible, cmdInstanceCount, lit);
    };

    // camera looking at the front grid
    const auto [visFront, cmdFront, litFront] = renderAndCount();
    std::fprintf(stderr, "gl_smoke_culling: front visible=%d cmdInstanceCount=%u litPx=%d\n", visFront, cmdFront, litFront);
    expect(visFront >= 400 && visFront <= 500, "GPU cull keeps the front instances (edge instances may be culled)");
    expect(cmdFront == static_cast<unsigned>(visFront), "indirect command instanceCount written by the GPU");
    expect(litFront > 200, "indirect draw lights pixels");

    // camera turned around (yaw = pi -> looks +Z): the back grid is visible now
    cam.yaw = 3.14159265f;
    const auto [visBack, cmdBack, litBack] = renderAndCount();
    std::fprintf(stderr, "gl_smoke_culling: back visible=%d cmdInstanceCount=%u litPx=%d\n", visBack, cmdBack, litBack);
    expect(visBack == 500, "turned camera keeps exactly the 500 back instances");
    expect(litBack > 200, "indirect draw lights pixels (back)");

    // camera pitched up 89 deg: everything is below the frustum -> nothing visible
    cam.yaw = 0.f;
    cam.pitch = 1.55f;
    const auto [visNone, cmdNone, litNone] = renderAndCount();
    std::fprintf(stderr, "gl_smoke_culling: none visible=%d cmdInstanceCount=%u litPx=%d\n", visNone, cmdNone, litNone);
    expect(visNone == 0, "looking away culls all instances");
    expect(cmdNone == 0u, "indirect command instanceCount is 0");
    expect(litNone == 0, "no pixels drawn when everything is culled");

    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_culling: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_culling: OK\n");
    return 0;
}
