// Screenshot tool for the tutorials: renders the demo scene (gradient skybox +
// rotating colored triangle) into an FBO via a headless surfaceless EGL context
// and writes a PNG. Used by CI to produce docs/images/*.png artifacts.
//
// Usage: screenshot --out file.png [--width 800] [--height 600] [--frames 30] [--camera fp|iso]

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/skybox.hpp>

#include "png_writer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr const char* kVs = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uViewProj;
out vec3 vColor;
void main() { gl_Position = uViewProj * vec4(aPos, 1.0); vColor = aColor; }
)GLSL";

constexpr const char* kFs = R"GLSL(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)GLSL";

struct Args {
    std::string out = "screenshot.png";
    int width = 800, height = 600, frames = 30;
    std::string camera = "fp";
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) a.out = argv[++i];
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) a.width = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) a.height = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) a.frames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--camera") == 0 && i + 1 < argc) a.camera = argv[++i];
    }
    return a;
}

} // namespace

int main(int argc, char** argv) {
    const Args args = parseArgs(argc, argv);
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("screenshot", args.width, args.height)) {
        std::fprintf(stderr, "screenshot: no GL context (headless without EGL?)\n");
        return 2;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "screenshot: context below GL 3.3\n");
        return 2;
    }

    fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
    if (!vs.compile(kVs) || !fs.compile(kFs)) {
        std::fprintf(stderr, "screenshot: shader failed: %s %s\n", vs.infoLog().c_str(), fs.infoLog().c_str());
        return 1;
    }
    fuhgl::Program prog;
    prog.create();
    prog.attach(vs);
    prog.attach(fs);
    if (!prog.link()) {
        std::fprintf(stderr, "screenshot: link failed: %s\n", prog.infoLog().c_str());
        return 1;
    }

    const float vertices[] = {
         0.0f,  0.6f, 0.0f,  1.0f, 0.2f, 0.2f,
        -0.6f, -0.4f, 0.0f,  0.2f, 1.0f, 0.2f,
         0.6f, -0.4f, 0.0f,  0.2f, 0.2f, 1.0f,
    };
    fuhgl::Buffer vbo(fuhgl::Buffer::Target::Array);
    vbo.bind();
    vbo.setData(sizeof(vertices), vertices, fuhgl::Buffer::Usage::StaticDraw);
    fuhgl::VertexArray vao;
    vao.create();
    vao.bind();
    vao.setAttrib(0, 3, fuhgl::gl::GL_FLOAT, false, 6 * sizeof(float), 0);
    vao.setAttrib(1, 3, fuhgl::gl::GL_FLOAT, false, 6 * sizeof(float), 3 * sizeof(float));

    fuhgl::Skybox skybox;
    skybox.create();
    skybox.makeGradient({0.25f, 0.45f, 0.9f}, {0.85f, 0.88f, 0.95f}, 256);

    fuhgl::FirstPersonCamera fpCam;
    fpCam.position = {0.f, 0.f, 3.f};
    fuhgl::IsometricCamera isoCam;
    fuhgl::Camera* cam = args.camera == "iso" ? static_cast<fuhgl::Camera*>(&isoCam)
                                              : static_cast<fuhgl::Camera*>(&fpCam);

    // Offscreen render target.
    fuhgl::Texture colorTex(fuhgl::Texture::Target::Tex2D);
    colorTex.bind();
    colorTex.texStorage2D(1, fuhgl::gl::GL_RGBA8, args.width, args.height);
    fuhgl::Renderbuffer depth;
    depth.create();
    depth.bind();
    depth.storage(fuhgl::gl::GL_DEPTH_COMPONENT24, args.width, args.height);
    fuhgl::Framebuffer fbo;
    fbo.create();
    fbo.bind();
    fbo.attachTexture2D(fuhgl::Framebuffer::Attachment::Color0, colorTex);
    fbo.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, depth);
    if (!fbo.isComplete()) {
        std::fprintf(stderr, "screenshot: FBO incomplete\n");
        return 1;
    }

    fuhgl::gl::enable(fuhgl::gl::Cap::DepthTest);
    const float aspect = static_cast<float>(args.width) / static_cast<float>(args.height);
    const float t = 0.6f; // fixed angle -> deterministic screenshot
    const fuhgl::math::Mat4 model = fuhgl::math::rotateY(t) * fuhgl::math::rotateX(0.3f * t);

    fuhgl::gl::viewport(0, 0, args.width, args.height);
    fuhgl::gl::clearColor(0.1f, 0.1f, 0.12f, 1.f);
    fuhgl::gl::clear(fuhgl::gl::ClearBit::Color | fuhgl::gl::ClearBit::Depth);
    skybox.draw(*cam, aspect);
    prog.use();
    prog.setUniform("uViewProj", cam->projection(aspect) * cam->view() * model);
    vao.bind();
    fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);

    std::vector<unsigned char> px(static_cast<size_t>(args.width) * args.height * 4);
    fuhgl::gl::readPixels(0, 0, args.width, args.height, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());

    // GL rows are bottom-up; flip for PNG (top-down).
    std::vector<unsigned char> flipped(px.size());
    const size_t stride = static_cast<size_t>(args.width) * 4;
    for (int y = 0; y < args.height; ++y)
        std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                    px.data() + static_cast<size_t>(args.height - 1 - y) * stride, stride);

    if (!screenshot::writePng(args.out.c_str(), flipped.data(), args.width, args.height)) {
        std::fprintf(stderr, "screenshot: failed to write %s\n", args.out.c_str());
        return 1;
    }
    std::fprintf(stderr, "screenshot: wrote %s (%dx%d, GL %d.%d)\n", args.out.c_str(), args.width, args.height,
                 fuhgl::gl::version().major, fuhgl::gl::version().minor);
    return 0;
}
