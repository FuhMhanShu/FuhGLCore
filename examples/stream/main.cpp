// Buffer/texture power-ups demo (F9): left half = triangle whose vertex colors are
// streamed every frame via Buffer::mapRange(GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
// right half = quad with a procedurally generated DXT1-compressed texture uploaded via
// Texture::compressedImage2D. Windowed by default; with --screenshot it renders one
// deterministic frame headless (surfaceless EGL) into an FBO and writes a PNG —
// used by the CI docs job for the tutorial screenshots.
//
// Usage: stream [--screenshot out.png] [--width W] [--height H]

#include <fuhgl/fuhgl.hpp>

#include "../screenshot/png_writer.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Args {
    std::string screenshot;
    int width = 800, height = 600;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) a.screenshot = argv[++i];
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) a.width = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) a.height = std::atoi(argv[++i]);
    }
    return a;
}

constexpr const char* kTriVs = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aColor;
out vec3 vColor;
void main() { vColor = aColor; gl_Position = vec4(aPos, 0.0, 1.0); }
)GLSL";

constexpr const char* kTriFs = R"GLSL(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)GLSL";

constexpr const char* kQuadVs = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() { vUV = aUV; gl_Position = vec4(aPos, 0.0, 1.0); }
)GLSL";

constexpr const char* kQuadFs = R"GLSL(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 FragColor;
void main() { FragColor = texture(uTex, vUV); }
)GLSL";

struct TriVert { float p[2]; float c[3]; };

// Writes one frame of animated vertex colors into the mapped VBO range.
void streamColors(void* mapped, float t) {
    auto* v = static_cast<TriVert*>(mapped);
    v[0].p[0] = -0.8f; v[0].p[1] = -0.8f;
    v[1].p[0] = 0.8f;  v[1].p[1] = -0.8f;
    v[2].p[0] = 0.0f;  v[2].p[1] = 0.8f;
    const float r = 0.5f + 0.5f * std::sin(t);
    const float g = 0.5f + 0.5f * std::sin(t + 2.094f);
    const float b = 0.5f + 0.5f * std::sin(t + 4.188f);
    v[0].c[0] = r; v[0].c[1] = 0.f; v[0].c[2] = 1.f - r;
    v[1].c[0] = 0.f; v[1].c[1] = g; v[1].c[2] = 1.f - g;
    v[2].c[0] = 1.f - b; v[2].c[1] = b; v[2].c[2] = 0.5f;
}

// Builds a 64x64 checkerboard as DXT1 (16x16 blocks * 8 bytes). Each block is solid
// (index 0 -> color0), so the checker pattern comes from the per-block color0.
std::vector<unsigned char> makeDxt1Checker(int size, bool& ok) {
    const int blocks = (size / 4) * (size / 4);
    std::vector<unsigned char> data(static_cast<size_t>(blocks) * 8);
    auto rgb565 = [](float r, float g, float b) -> unsigned short {
        return static_cast<unsigned short>((static_cast<int>(r * 31.f) << 11) |
                                           (static_cast<int>(g * 63.f) << 5) |
                                           static_cast<int>(b * 31.f));
    };
    int bi = 0;
    for (int by = 0; by < size / 4; ++by)
        for (int bx = 0; bx < size / 4; ++bx, ++bi) {
            const bool dark = ((bx + by) % 2) == 0;
            const unsigned short c0 = dark ? rgb565(0.95f, 0.45f, 0.15f) : rgb565(0.15f, 0.55f, 0.95f);
            unsigned char* blk = data.data() + static_cast<size_t>(bi) * 8;
            blk[0] = static_cast<unsigned char>(c0 & 0xFF);
            blk[1] = static_cast<unsigned char>(c0 >> 8);
            blk[2] = 0; blk[3] = 0; // color1 = 0 < color0 -> 4-color mode, index 0 selects color0
            blk[4] = blk[5] = blk[6] = blk[7] = 0; // all 16 texel indices = 0
        }
    ok = true;
    return data;
}

} // namespace

int main(int argc, char** argv) {
    const Args args = parseArgs(argc, argv);
    const bool headless = !args.screenshot.empty();
    if (headless) {
#ifndef _WIN32
        if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
        if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif
    }

    fuhgl::Window window;
    if (!window.create(headless ? "stream-screenshot" : "FuhGLCore - buffer/texture power-ups",
                       args.width, args.height)) {
        std::fprintf(stderr, "stream: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "stream: need GL >= 3.3\n");
        return 2;
    }

    // --- triangle program + streamable VBO ---------------------------------------
    fuhgl::Shader triVs(fuhgl::Shader::Type::Vertex), triFs(fuhgl::Shader::Type::Fragment);
    if (!triVs.compile(kTriVs) || !triFs.compile(kTriFs)) {
        std::fprintf(stderr, "stream: tri shaders: %s %s\n", triVs.infoLog().c_str(), triFs.infoLog().c_str());
        return 1;
    }
    fuhgl::Program triProg;
    triProg.create();
    triProg.attach(triVs);
    triProg.attach(triFs);
    if (!triProg.link()) { std::fprintf(stderr, "stream: tri link: %s\n", triProg.infoLog().c_str()); return 1; }

    fuhgl::Buffer triVbo(fuhgl::Buffer::Target::Array);
    triVbo.bind();
    triVbo.setData(3 * sizeof(TriVert), nullptr, fuhgl::Buffer::Usage::StreamDraw);
    fuhgl::VertexArray triVao;
    triVao.create();
    triVao.bind();
    triVao.setAttrib(0, 2, fuhgl::gl::GL_FLOAT, false, sizeof(TriVert), offsetof(TriVert, p));
    triVao.setAttrib(1, 3, fuhgl::gl::GL_FLOAT, false, sizeof(TriVert), offsetof(TriVert, c));

    // --- quad program + DXT1 texture ----------------------------------------------
    fuhgl::Shader quadVs(fuhgl::Shader::Type::Vertex), quadFs(fuhgl::Shader::Type::Fragment);
    if (!quadVs.compile(kQuadVs) || !quadFs.compile(kQuadFs)) {
        std::fprintf(stderr, "stream: quad shaders: %s %s\n", quadVs.infoLog().c_str(), quadFs.infoLog().c_str());
        return 1;
    }
    fuhgl::Program quadProg;
    quadProg.create();
    quadProg.attach(quadVs);
    quadProg.attach(quadFs);
    if (!quadProg.link()) { std::fprintf(stderr, "stream: quad link: %s\n", quadProg.infoLog().c_str()); return 1; }

    struct QuadVert { float p[2]; float uv[2]; };
    const QuadVert quadVerts[4] = {
        {{-0.8f, -0.8f}, {0.f, 0.f}}, {{0.8f, -0.8f}, {1.f, 0.f}},
        {{0.8f, 0.8f}, {1.f, 1.f}},   {{-0.8f, 0.8f}, {0.f, 1.f}},
    };
    const unsigned int quadIdx[6] = {0, 1, 2, 0, 2, 3};
    fuhgl::Buffer quadVbo(fuhgl::Buffer::Target::Array);
    quadVbo.bind();
    quadVbo.setData(sizeof quadVerts, quadVerts, fuhgl::Buffer::Usage::StaticDraw);
    fuhgl::Buffer quadIbo(fuhgl::Buffer::Target::ElementArray);
    quadIbo.bind();
    quadIbo.setData(sizeof quadIdx, quadIdx, fuhgl::Buffer::Usage::StaticDraw);
    fuhgl::VertexArray quadVao;
    quadVao.create();
    quadVao.bind();
    quadVao.setAttrib(0, 2, fuhgl::gl::GL_FLOAT, false, sizeof(QuadVert), offsetof(QuadVert, p));
    quadVao.setAttrib(1, 2, fuhgl::gl::GL_FLOAT, false, sizeof(QuadVert), offsetof(QuadVert, uv));
    quadVao.setElementBuffer(quadIbo);

    fuhgl::Texture tex(fuhgl::Texture::Target::Tex2D);
    tex.bind();
    bool dxtOk = false;
    const std::vector<unsigned char> dxt = makeDxt1Checker(64, dxtOk);
    tex.compressedImage2D(0, fuhgl::gl::GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 64, 64,
                          static_cast<fuhgl::gl::GLsizei>(dxt.size()), dxt.data());
    if (fuhgl::gl::lastError() != fuhgl::gl::Error::NoError) {
        // driver without S3TC: fall back to an uncompressed checker
        std::fprintf(stderr, "stream: compressed upload unsupported — using RGBA fallback\n");
        std::vector<unsigned char> rgba(64 * 64 * 4);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                const bool dark = ((x / 4 + y / 4) % 2) == 0;
                unsigned char* px = rgba.data() + (static_cast<size_t>(y) * 64 + x) * 4;
                if (dark) { px[0] = 242; px[1] = 115; px[2] = 38; }
                else      { px[0] = 38;  px[1] = 140; px[2] = 242; }
                px[3] = 255;
            }
        tex.setImage2D(0, fuhgl::gl::GL_RGBA8, 64, 64, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, rgba.data());
    }
    tex.setParameteri(fuhgl::gl::GL_TEXTURE_MIN_FILTER, fuhgl::gl::GL_LINEAR);
    tex.setParameteri(fuhgl::gl::GL_TEXTURE_MAG_FILTER, fuhgl::gl::GL_LINEAR);
    tex.setParameteri(fuhgl::gl::GL_TEXTURE_WRAP_S, fuhgl::gl::GL_CLAMP_TO_EDGE);
    tex.setParameteri(fuhgl::gl::GL_TEXTURE_WRAP_T, fuhgl::gl::GL_CLAMP_TO_EDGE);

    fuhgl::gl::enable(fuhgl::gl::Cap::DepthTest);

    auto renderFrame = [&](float t) {
        // stream the triangle colors (orphaning via INVALIDATE_BUFFER_BIT)
        triVbo.bind();
        if (void* p = triVbo.mapRange(0, 3 * sizeof(TriVert),
                                      fuhgl::gl::GL_MAP_WRITE_BIT | fuhgl::gl::GL_MAP_INVALIDATE_BUFFER_BIT)) {
            streamColors(p, t);
            triVbo.unmap();
        } else {
            // very old driver without mapRange: plain subdata update
            TriVert v[3];
            streamColors(v, t);
            triVbo.setSubData(0, sizeof v, v);
        }

        const int w = headless ? args.width : window.framebufferWidth();
        const int h = headless ? args.height : window.framebufferHeight();
        fuhgl::gl::viewport(0, 0, w, h);
        fuhgl::gl::clearColor(0.05f, 0.05f, 0.08f, 1.f);
        fuhgl::gl::clear(fuhgl::gl::ClearBit::Color | fuhgl::gl::ClearBit::Depth);

        fuhgl::gl::viewport(0, 0, w / 2, h);
        triProg.use();
        triVao.bind();
        fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);

        fuhgl::gl::viewport(w / 2, 0, w - w / 2, h);
        quadProg.use();
        quadProg.setUniform("uTex", 0);
        tex.bind(fuhgl::gl::GL_TEXTURE0);
        quadVao.bind();
        fuhgl::gl::drawElements(fuhgl::gl::Primitive::Triangles, 6, fuhgl::gl::GL_UNSIGNED_INT, nullptr);
    };

    if (headless) {
        fuhgl::Framebuffer fbo;
        fuhgl::Texture shot(fuhgl::Texture::Target::Tex2D);
        fuhgl::Renderbuffer depth;
        shot.bind();
        shot.texStorage2D(1, fuhgl::gl::GL_RGBA8, args.width, args.height);
        depth.create();
        depth.bind();
        depth.storage(fuhgl::gl::GL_DEPTH_COMPONENT24, args.width, args.height);
        fbo.create();
        fbo.setSize(args.width, args.height);
        fbo.bind();
        fbo.attachTexture2D(0, shot);
        fbo.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, depth);
        if (!fbo.isComplete()) { std::fprintf(stderr, "stream: shot FBO incomplete\n"); return 1; }

        renderFrame(0.7f); // deterministic frame
        fuhgl::gl::flush();
        fbo.bind();
        std::vector<unsigned char> px(static_cast<size_t>(args.width) * args.height * 4);
        fuhgl::gl::readPixels(0, 0, args.width, args.height, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        std::vector<unsigned char> flipped(px.size());
        const size_t stride = static_cast<size_t>(args.width) * 4;
        for (int y = 0; y < args.height; ++y)
            std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                        px.data() + static_cast<size_t>(args.height - 1 - y) * stride, stride);
        if (!screenshot::writePng(args.screenshot.c_str(), flipped.data(), args.width, args.height)) {
            std::fprintf(stderr, "stream: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "stream: wrote %s (%dx%d, GL %d.%d)\n", args.screenshot.c_str(),
                     args.width, args.height, fuhgl::gl::version().major, fuhgl::gl::version().minor);
        return 0;
    }

    window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
        if (action == fuhgl::Action::Press && key == fuhgl::Key::Escape) window.setShouldClose(true);
    });
    double last = window.time();
    while (!window.shouldClose()) {
        window.pollEvents();
        const double now = window.time();
        last = now;
        renderFrame(static_cast<float>(now) * 2.f);
        window.swapBuffers();
    }
    (void)last;
    return 0;
}
