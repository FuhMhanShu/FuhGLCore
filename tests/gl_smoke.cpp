// Headless GL smoke test: creates a (surfaceless) window/context, renders a
// fullscreen triangle into an FBO, reads back the center pixel and verifies it.
// Skips cleanly (exit 0) when no GL implementation is available (e.g. no Mesa).

#include <fuhgl/fuhgl.hpp>

#include <cstdio>
#include <cstdlib>

namespace {

constexpr const char* kVs = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
)GLSL";

constexpr const char* kFs = R"GLSL(#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(1.0, 0.0, 0.0, 1.0); } // solid red
)GLSL";

} // namespace

int main() {
    // Force the headless path when running under the EGL backend.
    if (!std::getenv("FUHGL_BACKEND")) {
#ifdef _WIN32
        _putenv_s("FUHGL_BACKEND", "surfaceless");
#else
        setenv("FUHGL_BACKEND", "surfaceless", 0);
#endif
    }

    fuhgl::Window window;
    if (!window.create("gl_smoke", 64, 64)) {
        std::fprintf(stderr, "gl_smoke: no GL context available (headless without EGL) — SKIP\n");
        return 77; // skip, not a failure
    }
    std::fprintf(stderr, "gl_smoke: GL %d.%d\n", fuhgl::gl::version().major, fuhgl::gl::version().minor);
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke: context is below GL 3.3 (legacy fallback) — SKIP\n");
        return 77; // skip, not a failure
    }

    // device info / capabilities of the real context
    const fuhgl::gl::DeviceInfo info = fuhgl::gl::queryDeviceInfo();
    std::fprintf(stderr, "gl_smoke: %s | %s | GLSL %s | %zu extensions\n", info.vendor.c_str(),
                 info.renderer.c_str(), info.glslVersion.c_str(), info.extensions.size());
    bool infoOk = !info.vendor.empty() && !info.renderer.empty() && !info.versionString.empty();
    infoOk = infoOk && info.version.atLeast(3, 3);
    infoOk = infoOk && !info.extensions.empty() && info.hasExtension(info.extensions.front().c_str());
    infoOk = infoOk && info.limits.maxTextureSize2D >= 1024;
    infoOk = infoOk && info.limits.maxVertexAttribs >= 16;
    infoOk = infoOk && info.limits.maxDrawBuffers >= 8 && info.limits.maxColorAttachments >= 8; // GL 3.3 guarantees
    infoOk = infoOk && info.limits.maxSamples > 0 && info.limits.maxUniformBufferBindings > 0;
    infoOk = infoOk && info.features.dsa == fuhgl::gl::hasDsa();
    infoOk = infoOk && !info.report().empty();
    if (!infoOk) {
        std::fprintf(stderr, "gl_smoke: FAIL — device info query returned implausible values\n%s", info.report().c_str());
        return 1;
    }

    fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
    if (!vs.compile(kVs) || !fs.compile(kFs)) {
        std::fprintf(stderr, "gl_smoke: shader compile failed: %s %s\n", vs.infoLog().c_str(), fs.infoLog().c_str());
        return 1;
    }
    fuhgl::Program prog;
    prog.create();
    prog.attach(vs);
    prog.attach(fs);
    if (!prog.link()) {
        std::fprintf(stderr, "gl_smoke: link failed: %s\n", prog.infoLog().c_str());
        return 1;
    }

    const float tri[] = {-1.f, -1.f, 3.f, -1.f, -1.f, 3.f};
    fuhgl::Buffer vbo(fuhgl::Buffer::Target::Array);
    vbo.bind();
    vbo.setData(sizeof(tri), tri, fuhgl::Buffer::Usage::StaticDraw);
    fuhgl::VertexArray vao;
    vao.create();
    vao.bind();
    vao.setAttrib(0, 2, fuhgl::gl::GL_FLOAT, false, 2 * sizeof(float), 0);

    fuhgl::Texture colorTex(fuhgl::Texture::Target::Tex2D);
    colorTex.bind();
    colorTex.texStorage2D(1, fuhgl::gl::GL_RGBA8, 64, 64);
    fuhgl::Renderbuffer depth;
    depth.create();
    depth.bind();
    depth.storage(fuhgl::gl::GL_DEPTH_COMPONENT24, 64, 64);

    fuhgl::Framebuffer fbo;
    fbo.create();
    fbo.bind();
    fbo.attachTexture2D(fuhgl::Framebuffer::Attachment::Color0, colorTex);
    fbo.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, depth);
    if (!fbo.isComplete()) {
        std::fprintf(stderr, "gl_smoke: FBO incomplete (0x%x)\n", fbo.status());
        return 1;
    }

    fuhgl::gl::viewport(0, 0, 64, 64);
    fuhgl::gl::clearColor(0.f, 0.f, 1.f, 1.f); // blue background
    fuhgl::gl::clear(fuhgl::gl::ClearBit::Color | fuhgl::gl::ClearBit::Depth);
    prog.use();
    vao.bind();
    fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);

    unsigned char px[4] = {0, 0, 0, 0};
    fuhgl::gl::readPixels(32, 32, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px);
    std::fprintf(stderr, "gl_smoke: center pixel = (%d, %d, %d, %d)\n", px[0], px[1], px[2], px[3]);

    const bool ok = px[0] > 200 && px[1] < 60 && px[2] < 60 && px[3] == 255;
    if (!ok) std::fprintf(stderr, "gl_smoke: FAIL — expected red triangle on blue background\n");
    return ok ? 0 : 1;
}
