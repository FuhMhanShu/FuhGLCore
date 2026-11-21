// Headless smoke test for F35 GL_ARB_bindless_texture: an 8x8 RGBA texture is turned into
// a resident 64-bit handle (ext::BindlessTexture) and sampled in a fragment shader via
// `uniform uint64_t uTex` + sampler2D(uTex) — no texture unit binding. The center pixel
// must match the texture color. Skips (77) without GL >= 3.3 or without the extension
// (e.g. macOS).

#include <fuhgl/fuhgl.hpp>

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
out vec2 vUV;
void main() {
    // fullscreen triangle
    vUV = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    gl_Position = vec4(vUV, 0.0, 1.0);
    vUV = vUV * 0.5 + 0.5;
}
)GLSL";

constexpr const char* kFs = R"GLSL(#version 330 core
#extension GL_ARB_bindless_texture : require
in vec2 vUV;
out vec4 FragColor;
uniform uint64_t uTex;
void main() { FragColor = texture(sampler2D(uTex), vUV); }
)GLSL";

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_bindless", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_bindless: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_bindless: context below GL 3.3 — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::ext::supported()) {
        std::fprintf(stderr, "gl_smoke_bindless: GL_ARB_bindless_texture not available — SKIP\n");
        return 77;
    }

    const int W = 64, H = 64;

    // 8x8 solid texture, color (200, 30, 40)
    std::vector<unsigned char> img(8 * 8 * 4);
    for (size_t i = 0; i < 8 * 8; ++i) {
        img[i * 4 + 0] = 200;
        img[i * 4 + 1] = 30;
        img[i * 4 + 2] = 40;
        img[i * 4 + 3] = 255;
    }
    fuhgl::Texture tex(fuhgl::Texture::Target::Tex2D);
    tex.bind();
    tex.setImage2D(0, fuhgl::gl::GL_RGBA8, 8, 8, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, img.data());
    tex.setParameteri(fuhgl::gl::GL_TEXTURE_MIN_FILTER, fuhgl::gl::GL_NEAREST);
    tex.setParameteri(fuhgl::gl::GL_TEXTURE_MAG_FILTER, fuhgl::gl::GL_NEAREST);
    tex.setParameteri(fuhgl::gl::GL_TEXTURE_WRAP_S, fuhgl::gl::GL_CLAMP_TO_EDGE);
    tex.setParameteri(fuhgl::gl::GL_TEXTURE_WRAP_T, fuhgl::gl::GL_CLAMP_TO_EDGE);

    fuhgl::gl::ext::BindlessTexture bt;
    expect(bt.attach(tex), "bindless handle created + resident");
    expect(bt.handle() != 0, "handle is non-zero");
    expect(bt.isResident(), "handle reports resident");

    fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
    if (!vs.compile(kVs) || !fs.compile(kFs)) {
        std::fprintf(stderr, "gl_smoke_bindless: shader compile failed: %s | %s\n", vs.infoLog().c_str(), fs.infoLog().c_str());
        return 1;
    }
    fuhgl::Program prog;
    prog.create();
    prog.attach(vs);
    prog.attach(fs);
    if (!prog.link()) {
        std::fprintf(stderr, "gl_smoke_bindless: link failed: %s\n", prog.infoLog().c_str());
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
    const float clear[4] = {0.f, 0.f, 0.f, 1.f};
    fuhgl::gl::clearBufferfv(fuhgl::gl::GL_COLOR, 0, clear);

    prog.use();
    fuhgl::gl::ext::setUniformHandle(prog, "uTex", bt.handle());
    fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);
    fuhgl::gl::flush();

    unsigned char px[4] = {0, 0, 0, 0};
    fuhgl::gl::readPixels(W / 2, H / 2, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px);
    std::fprintf(stderr, "gl_smoke_bindless: center=(%d,%d,%d,%d)\n", px[0], px[1], px[2], px[3]);
    expect(std::abs(static_cast<int>(px[0]) - 200) < 12 && std::abs(static_cast<int>(px[1]) - 30) < 12 &&
               std::abs(static_cast<int>(px[2]) - 40) < 12,
           "fragment shader sampled the texture through the bindless handle");

    bt.detach();
    expect(bt.handle() == 0 && !bt.isResident(), "detach makes the handle non-resident");

    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_bindless: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_bindless: OK\n");
    return 0;
}
