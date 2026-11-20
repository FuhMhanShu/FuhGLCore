// FuhGLCore example: a rotating, colored triangle in a native window.
// Controls: mouse look (all cameras), WASD + Space/Ctrl to move (FP/Free),
// Q/E roll (Free), right-drag pan + wheel zoom (Isometric).
// Keys 1/2/3 switch camera: FirstPerson / Free / Isometric. Escape quits.

#include <fuhgl/fuhgl.hpp>

#include <cstdio>

namespace {

constexpr const char* kVertexSrc = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uViewProj;
out vec3 vColor;
void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
    vColor = aColor;
}
)GLSL";

constexpr const char* kFragmentSrc = R"GLSL(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)GLSL";

} // namespace

int main() {
    fuhgl::Window window;
    if (!window.create("FuhGLCore - triangle", 800, 600)) {
        std::fprintf(stderr, "Failed to create window/GL context (headless?)\n");
        return 1;
    }
    std::fprintf(stderr, "GL version: %d.%d\n", fuhgl::gl::version().major, fuhgl::gl::version().minor);

    fuhgl::gl::setDebugCallback([](const fuhgl::gl::DebugMessage& msg) {
        std::fprintf(stderr, "[GL debug] type=0x%x severity=0x%x: %s\n",
                     msg.type, msg.severity, msg.message.c_str());
    });

    fuhgl::Shader vs(fuhgl::Shader::Type::Vertex);
    fuhgl::Shader fs(fuhgl::Shader::Type::Fragment);
    if (!vs.compile(kVertexSrc)) { std::fprintf(stderr, "VS: %s\n", vs.infoLog().c_str()); return 1; }
    if (!fs.compile(kFragmentSrc)) { std::fprintf(stderr, "FS: %s\n", fs.infoLog().c_str()); return 1; }

    fuhgl::Program program;
    program.create();
    program.attach(vs);
    program.attach(fs);
    if (!program.link()) { std::fprintf(stderr, "Link: %s\n", program.infoLog().c_str()); return 1; }

    const float vertices[] = {
        // position          // color
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

    fuhgl::FirstPersonCamera fpCam;
    fpCam.position = {0.f, 0.f, 3.f};
    fuhgl::FreeCamera freeCam;
    freeCam.position = {0.f, 0.f, 3.f};
    fuhgl::IsometricCamera isoCam;

    fuhgl::Camera* cam = &fpCam;
    fuhgl::Camera* pendingCam = nullptr; // applied after pollEvents (never re-enter the callback)
    cam->attach(window);
    window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
        if (action == fuhgl::Action::Press && key == fuhgl::Key::Escape) window.setShouldClose(true);
        if (action == fuhgl::Action::Press) {
            if (key == fuhgl::Key::Digit1) pendingCam = &fpCam;
            if (key == fuhgl::Key::Digit2) pendingCam = &freeCam;
            if (key == fuhgl::Key::Digit3) pendingCam = &isoCam;
        }
        cam->onKey(key, action);
    });

    fuhgl::gl::enable(fuhgl::gl::Cap::DepthTest);
    fuhgl::gl::clearColor(0.1f, 0.1f, 0.12f, 1.f);

    double lastTime = window.time();
    while (!window.shouldClose()) {
        window.pollEvents();
        if (pendingCam && pendingCam != cam) {
            cam = pendingCam;
            cam->attach(window); // rewires input callbacks to the new camera
            pendingCam = nullptr;
        }
        const double now = window.time();
        const float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        cam->update(dt);

        const float aspect = static_cast<float>(window.framebufferWidth()) /
                             static_cast<float>(window.framebufferHeight());
        fuhgl::gl::viewport(0, 0, window.framebufferWidth(), window.framebufferHeight());
        fuhgl::gl::clear(fuhgl::gl::ClearBit::Color | fuhgl::gl::ClearBit::Depth);

        program.use();
        const float t = static_cast<float>(now);
        const fuhgl::math::Mat4 model = fuhgl::math::rotateY(t) * fuhgl::math::rotateX(0.3f * t);
        program.setUniform("uViewProj", cam->projection(aspect) * cam->view() * model);

        vao.bind();
        fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);

        window.swapBuffers();
    }
    return 0;
}
