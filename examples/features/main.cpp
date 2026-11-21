// Screenshot scenes for the feature tutorials (30-38): each --scene renders one
// deterministic frame headless (surfaceless EGL) and writes a PNG, used by CI to
// produce docs/images/tutorial-*.png artifacts.
//
// Usage: features --screenshot out.png --scene <name> [--width W] [--height H]
// Scenes: noise | pbr | ibl | csm | taa | ssr | culling | bindless

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>
#include <fuhgl/skybox.hpp>

#include "../screenshot/png_writer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Args {
    std::string out = "features.png";
    std::string scene = "pbr";
    int width = 800, height = 600;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) a.out = argv[++i];
        else if (std::strcmp(argv[i], "--scene") == 0 && i + 1 < argc) a.scene = argv[++i];
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) a.width = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) a.height = std::atoi(argv[++i]);
    }
    return a;
}

bool writeShot(const Args& args, fuhgl::Framebuffer& fbo) {
    const int w = args.width, h = args.height;
    fuhgl::gl::flush();
    fbo.bind();
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
    fuhgl::gl::readPixels(0, 0, w, h, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
    std::vector<unsigned char> flipped(px.size());
    const size_t stride = static_cast<size_t>(w) * 4;
    for (int y = 0; y < h; ++y)
        std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                    px.data() + static_cast<size_t>(h - 1 - y) * stride, stride);
    if (!screenshot::writePng(args.out.c_str(), flipped.data(), w, h)) {
        std::fprintf(stderr, "features: failed to write %s\n", args.out.c_str());
        return false;
    }
    std::fprintf(stderr, "features: wrote %s (scene=%s, %dx%d)\n", args.out.c_str(), args.scene.c_str(), w, h);
    return true;
}

// fullscreen-triangle shader pair for the texture scenes (noise / bindless)
constexpr const char* kFsVs = R"GLSL(#version 330 core
out vec2 vUV;
void main() {
    vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    vUV = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
)GLSL";
constexpr const char* kNoiseFs = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
void main() { FragColor = vec4(texture(uTex, vUV).rgb, 1.0); }
)GLSL";
constexpr const char* kBindlessFs = R"GLSL(#version 330 core
#extension GL_ARB_bindless_texture : require
in vec2 vUV;
out vec4 FragColor;
uniform uint64_t uTex;
void main() { FragColor = vec4(texture(sampler2D(uTex), vUV).rgb, 1.0); }
)GLSL";

bool linkProg(fuhgl::Program& prog, const char* vsSrc, const char* fsSrc) {
    fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
    if (!vs.compile(vsSrc) || !fs.compile(fsSrc)) {
        std::fprintf(stderr, "features: shader failed: %s | %s\n", vs.infoLog().c_str(), fs.infoLog().c_str());
        return false;
    }
    if (!prog.create()) return false;
    prog.attach(vs);
    prog.attach(fs);
    if (!prog.link()) {
        std::fprintf(stderr, "features: link failed: %s\n", prog.infoLog().c_str());
        return false;
    }
    return true;
}

// raw texture scene (noise / bindless): fullscreen triangle into an FBO
bool sceneTexture(const Args& args, bool bindless) {
    const int W = args.width, H = args.height;
    fuhgl::Texture tex = fuhgl::render::procedural::makeTexture(
        fuhgl::render::procedural::noise(256, 256, 7u, 5));
    tex.bind(0);
    tex.setParameteri(fuhgl::gl::GL_TEXTURE_MIN_FILTER, fuhgl::gl::GL_LINEAR);
    tex.setParameteri(fuhgl::gl::GL_TEXTURE_MAG_FILTER, fuhgl::gl::GL_LINEAR);

    fuhgl::Program prog;
    fuhgl::gl::ext::BindlessTexture handle;
    if (bindless && fuhgl::gl::ext::supported() && handle.attach(tex)) {
        if (!linkProg(prog, kFsVs, kBindlessFs)) return false;
    } else {
        if (bindless)
            std::fprintf(stderr, "features: bindless unsupported, falling back to unit binding\n");
        if (!linkProg(prog, kFsVs, kNoiseFs)) return false;
    }

    fuhgl::Texture colorTex(fuhgl::Texture::Target::Tex2D);
    colorTex.bind();
    colorTex.texStorage2D(1, fuhgl::gl::GL_RGBA8, W, H);
    fuhgl::Framebuffer fbo;
    fbo.create();
    fbo.setSize(W, H);
    fbo.bind();
    fbo.attachTexture2D(0, colorTex);
    if (!fbo.isComplete()) return false;
    fuhgl::gl::viewport(0, 0, W, H);
    fuhgl::gl::disable(fuhgl::gl::Cap::DepthTest);
    prog.use();
    if (bindless && handle.handle() != 0)
        fuhgl::gl::ext::setUniformHandle(prog, "uTex", handle.handle());
    else
        prog.setUniform(prog.uniformLocation("uTex"), 0);
    fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);
    return writeShot(args, fbo);
}

// deferred-renderer helper: common floor + sun scene builder
struct DefScene {
    fuhgl::render::DeferredRenderer renderer;
    fuhgl::render::SceneView scene;
    fuhgl::FirstPersonCamera cam;
    fuhgl::render::Mesh floorMesh, boxA, boxB, boxC;
    fuhgl::Texture bricksTex;
    fuhgl::render::Material floorMat;
};

bool buildDefScene(DefScene& d, int W, int H, bool bricksFloor) {
    using namespace fuhgl::render;
    if (!d.renderer.create(W, H)) return false;
    d.renderer.ssaoEnabled = false;
    d.renderer.godRaysEnabled = false;
    d.renderer.bloomStrength = 0.6f;
    d.cam.position = {0.f, 1.6f, 4.2f};
    d.cam.pitch = -0.30f;
    d.scene.camera = &d.cam;
    d.scene.width = W;
    d.scene.height = H;
    d.scene.aspect = static_cast<float>(W) / H;
    d.scene.clearColor = {0.05f, 0.07f, 0.12f};
    d.scene.ambient = {0.16f, 0.17f, 0.20f};
    shapes::box(d.floorMesh, {6.f, 0.25f, 6.f}, {1.f, 1.f, 1.f});
    if (bricksFloor) {
        d.bricksTex = procedural::makeTexture(procedural::bricks(256, 256, 6, 8, {0.55f, 0.30f, 0.22f}, {0.75f, 0.72f, 0.68f}));
        d.floorMat = materials::pbrTextured(d.bricksTex, {1.f, 1.f, 1.f}, 0.f, 0.85f);
    } else {
        d.floorMat = materials::pbr({0.45f, 0.46f, 0.50f}, 0.f, 0.9f);
    }
    d.scene.items.push_back({&d.floorMesh, &d.floorMat, fuhgl::math::translate({0.f, -0.25f, 0.f})});
    d.scene.lights.push_back(Light::directional({-0.4f, -1.f, -0.25f}, {1.f, 0.96f, 0.88f}, 2.2f));
    return true;
}

bool renderDef(DefScene& d, const Args& args, int frames) {
    for (int i = 0; i < frames; ++i) d.renderer.render(d.scene);
    return writeShot(args, d.renderer.output());
}

bool scenePbr(const Args& args) {
    using namespace fuhgl::render;
    DefScene d;
    if (!buildDefScene(d, args.width, args.height, false)) return false;
    shapes::box(d.boxA, {0.55f, 0.55f, 0.55f}, {1.f, 1.f, 1.f});
    shapes::box(d.boxB, {0.55f, 0.55f, 0.55f}, {1.f, 1.f, 1.f});
    shapes::box(d.boxC, {0.55f, 0.55f, 0.55f}, {1.f, 1.f, 1.f});
    Material dielectric = materials::pbr({0.75f, 0.20f, 0.15f}, 0.0f, 0.85f); // rough red plastic
    Material metal = materials::pbr({0.75f, 0.76f, 0.78f}, 1.0f, 0.18f);      // polished metal
    Material glossy = materials::pbr({0.20f, 0.35f, 0.80f}, 0.1f, 0.30f);     // glossy blue
    d.scene.items.push_back({&d.boxA, &dielectric, fuhgl::math::translate({-1.5f, 0.55f, -0.5f})});
    d.scene.items.push_back({&d.boxB, &metal, fuhgl::math::translate({0.f, 0.55f, -1.2f})});
    d.scene.items.push_back({&d.boxC, &glossy, fuhgl::math::translate({1.5f, 0.55f, -0.5f})});
    return renderDef(d, args, 1);
}

bool sceneIbl(const Args& args) {
    using namespace fuhgl::render;
    DefScene d;
    if (!buildDefScene(d, args.width, args.height, true)) return false;
    // bake IBL from a procedural day-gradient cubemap
    fuhgl::Skybox sky;
    sky.create();
    sky.makeGradient({0.25f, 0.45f, 0.9f}, {0.85f, 0.88f, 0.95f}, 64);
    Environment env;
    if (!env.createFromCubemap(sky.texture())) {
        std::fprintf(stderr, "features: IBL bake failed\n");
        return false;
    }
    env.intensity = 1.2f;
    d.scene.env = &env;
    d.scene.ambient = {0.03f, 0.03f, 0.04f}; // ambient comes from the env now
    shapes::box(d.boxA, {0.55f, 0.55f, 0.55f}, {1.f, 1.f, 1.f});
    shapes::box(d.boxB, {0.55f, 0.55f, 0.55f}, {1.f, 1.f, 1.f});
    Material dielectric = materials::pbr({0.75f, 0.20f, 0.15f}, 0.0f, 0.6f);
    Material metal = materials::pbr({0.75f, 0.76f, 0.78f}, 1.0f, 0.35f);
    d.scene.items.push_back({&d.boxA, &dielectric, fuhgl::math::translate({-1.1f, 0.55f, -0.5f})});
    d.scene.items.push_back({&d.boxB, &metal, fuhgl::math::translate({1.1f, 0.55f, -0.5f})});
    return renderDef(d, args, 1);
}

bool sceneCsm(const Args& args) {
    using namespace fuhgl::render;
    DefScene d;
    if (!buildDefScene(d, args.width, args.height, true)) return false;
    d.renderer.enableCsmShadows(2048, 4);
    d.renderer.setShadowZRange(0.5f, 80.f);
    d.renderer.setShadowStrength(1.f);
    d.cam.position = {0.f, 7.f, 30.f};
    d.cam.pitch = -0.34f;
    // long ground + a row of casters at increasing distance -> cascades kick in
    shapes::box(d.floorMesh, {40.f, 0.25f, 40.f}, {1.f, 1.f, 1.f});
    d.scene.items[0] = {&d.floorMesh, &d.floorMat, fuhgl::math::translate({0.f, -0.25f, -10.f})};
    // low sun -> long shadows stretching across the cascades
    d.scene.lights[0] = Light::directional({0.55f, -0.42f, 0.25f}, {1.f, 0.9f, 0.75f}, 2.4f);
    for (int i = 0; i < 6; ++i) {
        shapes::box(d.boxA, {0.5f, 1.2f + 0.3f * i, 0.5f}, {1.f, 1.f, 1.f});
        // reuse one mesh per distance via separate meshes is overkill; use same box scaled
        d.scene.items.push_back({&d.boxA, nullptr,
                                 fuhgl::math::translate({-6.f + 2.4f * i, 0.6f + 0.15f * i, -2.f - 6.f * i}) *
                                 fuhgl::math::scale({1.f, 1.f + 0.25f * i, 1.f})});
    }
    return renderDef(d, args, 1);
}

bool sceneTaa(const Args& args) {
    using namespace fuhgl::render;
    DefScene d;
    if (!buildDefScene(d, args.width, args.height, true)) return false;
    d.renderer.taaEnabled = true;
    d.renderer.taaBlend = 0.12f;
    d.cam.position = {0.f, 1.1f, 2.4f};
    d.cam.pitch = -0.55f; // close-up on the brick floor: jittered edges get smoothed
    shapes::box(d.boxA, {0.4f, 0.8f, 0.4f}, {0.8f, 0.5f, 0.2f});
    d.scene.items.push_back({&d.boxA, nullptr, fuhgl::math::translate({0.6f, 0.4f, -1.2f})});
    return renderDef(d, args, 16); // let the history accumulate
}

bool sceneSsr(const Args& args) {
    using namespace fuhgl::render;
    DefScene d;
    if (!buildDefScene(d, args.width, args.height, false)) return false;
    d.renderer.ssrEnabled = true;
    d.renderer.ssrStrength = 1.0f;
    d.renderer.ssrMaxSteps = 48;
    d.renderer.ssrThickness = 0.6f;
    d.renderer.exposure = 0.85f;      // keep the floor from blowing out to white
    d.renderer.bloomThreshold = 0.9f;  // only the lamp glows
    d.renderer.bloomStrength = 0.8f;
    d.scene.ambient = {0.10f, 0.10f, 0.12f};
    // same framing as the gl_smoke_ssr test (proven to show the reflection band)
    d.cam.position = {0.f, 3.f, 7.f};
    d.cam.pitch = -0.45f;
    // bright emissive lamp whose reflection must appear in the floor
    shapes::box(d.boxA, {0.5f, 1.f, 0.5f}, {1.f, 1.f, 1.f});
    Material lamp = materials::emissive({1.f, 0.9f, 0.7f}, 6.f);
    d.scene.items.push_back({&d.boxA, &lamp, fuhgl::math::translate({-1.f, 1.f, -1.f})});
    shapes::box(d.boxB, {0.5f, 0.5f, 0.5f}, {0.3f, 0.4f, 0.6f});
    d.scene.items.push_back({&d.boxB, nullptr, fuhgl::math::translate({1.5f, 0.5f, 0.5f})});
    return renderDef(d, args, 1);
}

// GPU culling scene (F34): grid of boxes, half of them behind the camera
bool sceneCulling(const Args& args) {
    using namespace fuhgl::render;
    const int W = args.width, H = args.height;
    if (!fuhgl::gl::version().atLeast(4, 3)) {
        std::fprintf(stderr, "features: culling scene needs GL >= 4.3\n");
        return false;
    }
    Mesh boxMesh;
    shapes::box(boxMesh, {0.3f, 0.3f, 0.3f}, {1.f, 1.f, 1.f});
    GpuCuller culler;
    if (!culler.create(2000)) {
        std::fprintf(stderr, "features: culler create failed\n");
        return false;
    }
    culler.setMesh(boxMesh);
    std::vector<fuhgl::math::Mat4> models;
    models.reserve(2000);
    for (int i = 0; i < 1000; ++i) { // front grid 25x10x4 receding into the distance
        const float x = -8.f + 16.f * (i % 25) / 24.f;
        const float y = -1.f + 4.f * ((i / 25) % 10) / 9.f;
        const float z = -2.f - 18.f * (i / 250);
        const float s = 0.8f + 0.6f * ((i * 37 % 100) / 100.f);
        models.push_back(fuhgl::math::translate({x, y, z}) * fuhgl::math::scale({s, s, s}) *
                         fuhgl::math::rotateY(0.3f * i));
    }
    for (int i = 0; i < 1000; ++i) // behind the camera -> culled
        models.push_back(fuhgl::math::translate({0.f, 0.f, 4.f + 10.f * i / 1000.f}));
    culler.setInstances(models.data(), static_cast<int>(models.size()));

    constexpr const char* kVs = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 4) in mat4 aInstanceModel;
uniform mat4 uViewProj;
out vec3 vColor;
void main() {
    gl_Position = uViewProj * (aInstanceModel * vec4(aPos, 1.0));
    vColor = vec3(0.95f, 0.65f, 0.25f) * (0.6f + 0.4f * fract(aInstanceModel[3][0] * 7.13f));
}
)GLSL";
    constexpr const char* kFs = R"GLSL(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)GLSL";
    fuhgl::Program prog;
    if (!linkProg(prog, kVs, kFs)) return false;

    fuhgl::Texture colorTex(fuhgl::Texture::Target::Tex2D);
    colorTex.bind();
    colorTex.texStorage2D(1, fuhgl::gl::GL_RGBA8, W, H);
    fuhgl::Renderbuffer depth;
    depth.create();
    depth.bind();
    depth.storage(fuhgl::gl::GL_DEPTH_COMPONENT24, W, H);
    fuhgl::Framebuffer fbo;
    fbo.create();
    fbo.setSize(W, H);
    fbo.bind();
    fbo.attachTexture2D(0, colorTex);
    fbo.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, depth);
    if (!fbo.isComplete()) return false;
    fuhgl::gl::viewport(0, 0, W, H);
    fuhgl::gl::enable(fuhgl::gl::Cap::DepthTest);
    fuhgl::gl::disable(fuhgl::gl::Cap::CullFace);
    fuhgl::gl::clearColor(0.05f, 0.07f, 0.12f, 1.f);
    fuhgl::gl::clear(fuhgl::gl::ClearBit::Color | fuhgl::gl::ClearBit::Depth);

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 1.5f, 6.f};
    cam.pitch = -0.15f;
    cam.yaw = 0.f;
    culler.cull(cam.viewProj(static_cast<float>(W) / H));
    prog.use();
    prog.setUniform("uViewProj", cam.viewProj(static_cast<float>(W) / H));
    culler.draw();
    std::fprintf(stderr, "features: culling visible=%d of %d\n", culler.lastVisibleCount(), (int)models.size());
    return writeShot(args, fbo);
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
    if (!window.create("features", args.width, args.height)) {
        std::fprintf(stderr, "features: no GL context\n");
        return 2;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "features: context below GL 3.3\n");
        return 2;
    }

    bool ok = false;
    const std::string& s = args.scene;
    if (s == "noise") ok = sceneTexture(args, false);
    else if (s == "bindless") ok = sceneTexture(args, true);
    else if (s == "pbr") ok = scenePbr(args);
    else if (s == "ibl") ok = sceneIbl(args);
    else if (s == "csm") ok = sceneCsm(args);
    else if (s == "taa") ok = sceneTaa(args);
    else if (s == "ssr") ok = sceneSsr(args);
    else if (s == "culling") ok = sceneCulling(args);
    else {
        std::fprintf(stderr, "features: unknown scene '%s'\n", s.c_str());
        return 2;
    }
    return ok ? 0 : 1;
}
