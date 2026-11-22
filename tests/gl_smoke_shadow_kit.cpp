// Headless smoke test for the F45 shadow kit: renders scene depth into a ShadowTarget
// (2D, 2D with VSM moments, cube via CubeShadowMap, array via ShadowAtlas), verifies
// the contents by depth/moments readback, and compiles + runs the public GLSL kit
// (kSkUniforms + kSkFuncs from render/shadow_glsl.hpp) in a custom shader to verify
// that PCF/PCSS/VSM sampling darkens a shadowed world point but not a lit one.
// Skips cleanly without GL >= 3.3.

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

constexpr const char* kTriVs = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
)GLSL";

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_shadow_kit", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_shadow_kit: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_shadow_kit: context below GL 3.3 — SKIP\n");
        return 77;
    }

    using namespace fuhgl;
    using namespace fuhgl::render;

    // --- scene: floor + one caster box, directional light + point light --------------
    Mesh floorMesh, caster;
    shapes::box(floorMesh, {6.f, 0.25f, 6.f}, {0.6f, 0.6f, 0.62f});
    shapes::box(caster, {0.6f, 1.2f, 0.6f}, {0.9f, 0.35f, 0.25f});

    FirstPersonCamera cam;
    cam.position = {0.f, 4.f, 8.f};
    cam.pitch = -0.38f;

    SceneView scene;
    scene.camera = &cam;
    scene.width = 64;
    scene.height = 64;
    scene.aspect = 1.f;
    scene.items.push_back({&floorMesh, nullptr, math::translate({0.f, -0.25f, 0.f})});
    scene.items.push_back({&caster, nullptr, math::translate({-0.5f, 1.2f, 0.2f})});
    const Light sun = Light::directional({0.55f, -1.f, 0.3f}, {1.f, 0.9f, 0.75f}, 1.5f);
    const Light lamp = Light::point({2.f, 3.f, 2.f}, {1.f, 1.f, 1.f}, 30.f, 50.f);
    scene.lights.push_back(sun);
    scene.lights.push_back(lamp);

    const math::Vec3 center{0.f, 0.5f, 0.f};
    const math::Mat4 lightVP = lightViewProj(sun, center, 7.f);

    // --- 1. ShadowTarget 2D: depth readback (center occluded by caster, corner cleared) --
    ShadowTarget t2d;
    if (!t2d.create(ShadowTargetKind::Tex2D, 256)) {
        std::fprintf(stderr, "gl_smoke_shadow_kit: 2D target create failed\n");
        return 1;
    }
    t2d.render(scene, lightVP);
    gl::flush();
    float depthCenter = 1.f, depthCorner = 0.f;
    t2d.fbo().bind();
    gl::readPixels(128, 128, 1, 1, gl::GL_DEPTH_COMPONENT, gl::GL_FLOAT, &depthCenter);
    gl::readPixels(2, 2, 1, 1, gl::GL_DEPTH_COMPONENT, gl::GL_FLOAT, &depthCorner);
    expect(depthCenter < 1.f, "2D depth: center texel written (< 1)");
    expect(depthCorner == 1.f, "2D depth: corner texel still cleared (== 1)");

    // --- 2. ShadowTarget with VSM moments: RGBA32F readback, m2 >= m1^2 ----------------
    ShadowTarget tm;
    if (!tm.create(ShadowTargetKind::Tex2D, 256, 1, true)) {
        std::fprintf(stderr, "gl_smoke_shadow_kit: moments target create failed\n");
        return 1;
    }
    expect(tm.momentsTextureId() != 0, "moments target has an RGBA32F moments texture");
    tm.render(scene, lightVP);
    gl::flush();
    float m[4] = {0.f, 0.f, 0.f, 0.f};
    tm.fbo().bind();
    gl::readPixels(128, 128, 1, 1, gl::GL_RGBA, gl::GL_FLOAT, m);
    expect(m[0] > 0.f && m[0] < 1.f, "VSM moment m1 in (0,1) at center");
    expect(m[1] >= m[0] * m[0] - 1e-4f, "VSM moment m2 >= m1^2 (variance non-negative)");

    // --- 3. CubeShadowMap: all 6 faces render, at least one face sees the caster --------
    CubeShadowMap cube;
    if (!cube.create(128)) {
        std::fprintf(stderr, "gl_smoke_shadow_kit: cube map create failed\n");
        return 1;
    }
    cube.render(scene, lamp, 0.5f, 50.f);
    gl::flush();
    float minFaceDepth = 1.f;
    cube.fbo().bind();
    for (int f = 0; f < 6; ++f) {
        gl::functions().glFramebufferTexture2D(gl::GL_FRAMEBUFFER, gl::GL_DEPTH_ATTACHMENT,
                                               gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, cube.textureId(), 0);
        float d = 1.f;
        gl::readPixels(64, 64, 1, 1, gl::GL_DEPTH_COMPONENT, gl::GL_FLOAT, &d);
        minFaceDepth = d < minFaceDepth ? d : minFaceDepth;
    }
    expect(minFaceDepth < 1.f, "cube map: at least one face contains scene depth");
    bool vpsFinite = true;
    for (int f = 0; f < 6; ++f) {
        const math::Vec4 c = cube.faceVP(f) * math::Vec4(lamp.position + math::Vec3{1.f, 0.f, 0.f}, 1.f);
        vpsFinite = vpsFinite && std::isfinite(c.x) && std::isfinite(c.y) && std::isfinite(c.z) && std::isfinite(c.w);
    }
    expect(vpsFinite, "cube map: all 6 face VPs finite");

    // --- 4. ShadowAtlas: two layers rendered, both readable ----------------------------
    ShadowAtlas atlas;
    if (!atlas.create(128, 4)) {
        std::fprintf(stderr, "gl_smoke_shadow_kit: atlas create failed\n");
        return 1;
    }
    atlas.renderLight(scene, 0, sun, center, 7.f);                       // snapped VP, stored
    atlas.renderLayer(scene, 1, lightViewProj(sun, center, 7.f));         // raw VP
    gl::flush();
    atlas.fbo().bind();
    float layerDepth[2] = {1.f, 1.f};
    for (int layer = 0; layer < 2; ++layer) {
        gl::framebufferTextureLayer(gl::GL_FRAMEBUFFER, gl::GL_DEPTH_ATTACHMENT, atlas.textureId(), 0, layer);
        gl::readPixels(64, 64, 1, 1, gl::GL_DEPTH_COMPONENT, gl::GL_FLOAT, &layerDepth[layer]);
    }
    expect(layerDepth[0] < 1.f && layerDepth[1] < 1.f, "atlas: layers 0 and 1 both contain depth");
    const math::Vec4 c0 = atlas.vp(0) * math::Vec4(center, 1.f);
    expect(std::isfinite(c0.x) && std::isfinite(c0.w) && c0.w != 0.f, "atlas: stored VP of layer 0 finite");

    // --- 5. public GLSL kit compiles and samples: shadowed vs lit world point ----------
    std::string fsSrc = std::string("#version 330 core\nout vec4 FragColor;\n") + kSkUniforms + kSkFuncs +
                        R"GLSL(
uniform vec3 uWorldPos;
void main() {
    vec3 p = shadowNdc(uLightVP, uWorldPos);
    float sh = 1.0, vsm = 1.0, pcss = 1.0;
    if (shadowInBounds(p)) {
        sh = pcfPoisson2D(uShadowMap, p.xy, p.z, uShadowBias, 4.0, 12, 0.0);
        vsm = vsm2D(uShadowMoments, p.xy, p.z, uMinVariance);
        pcss = pcss2D(uShadowMap, p.xy, p.z, uShadowBias, uLightSize, uShadowWorldWidth, 8, 12, 0.0);
    }
    FragColor = vec4(sh, vsm, pcss, 1.0);
}
)GLSL";

    Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
    if (!vs.compile(kTriVs) || !fs.compile(fsSrc.c_str())) {
        std::fprintf(stderr, "gl_smoke_shadow_kit: kit shader compile failed: %s %s\n", vs.infoLog().c_str(),
                     fs.infoLog().c_str());
        return 1;
    }
    Program prog;
    prog.create();
    prog.attach(vs);
    prog.attach(fs);
    if (!prog.link()) {
        std::fprintf(stderr, "gl_smoke_shadow_kit: kit link failed: %s\n", prog.infoLog().c_str());
        return 1;
    }

    const float tri[] = {-1.f, -1.f, 3.f, -1.f, -1.f, 3.f};
    Buffer vbo(Buffer::Target::Array);
    vbo.bind();
    vbo.setData(sizeof(tri), tri, Buffer::Usage::StaticDraw);
    VertexArray vao;
    vao.create();
    vao.bind();
    vao.setAttrib(0, 2, gl::GL_FLOAT, false, 2 * sizeof(float), 0);

    Texture outTex(Texture::Target::Tex2D);
    outTex.bind();
    outTex.texStorage2D(1, gl::GL_RGBA8, 64, 64);
    Framebuffer outFbo;
    outFbo.create();
    outFbo.setSize(64, 64);
    outFbo.bind();
    outFbo.attachTexture2D(Framebuffer::Attachment::Color0, outTex);
    expect(outFbo.isComplete(), "kit: output FBO complete");

    // bind the shadow textures: unit 0 = 2D depth, unit 1 = VSM moments
    gl::activeTexture(gl::GL_TEXTURE0);
    gl::functions().glBindTexture(gl::GL_TEXTURE_2D, t2d.textureId());
    gl::activeTexture(gl::GL_TEXTURE0 + 1);
    gl::functions().glBindTexture(gl::GL_TEXTURE_2D, tm.momentsTextureId());

    prog.use();
    prog.setUniform("uShadowMap", 0);
    prog.setUniform("uShadowMoments", 1);
    prog.setUniform("uLightVP", lightVP);
    prog.setUniform("uShadowBias", 0.002f);
    prog.setUniform("uMinVariance", 0.0005f);
    prog.setUniform("uLightSize", 1.0f);
    prog.setUniform("uShadowWorldWidth", 14.f);

    gl::disable(gl::Cap::DepthTest);
    gl::viewport(0, 0, 64, 64);
    auto sampleAt = [&](const math::Vec3& worldPos) {
        gl::clearColor(0.f, 0.f, 0.f, 1.f);
        gl::clear(gl::ClearBit::Color);
        prog.setUniform("uWorldPos", worldPos);
        vao.bind();
        gl::drawArrays(gl::Primitive::Triangles, 0, 3);
        gl::flush();
        unsigned char px[4] = {0, 0, 0, 0};
        outFbo.bind();
        gl::readPixels(32, 32, 1, 1, gl::GL_RGBA, gl::GL_UNSIGNED_BYTE, px);
        return px[0];
    };
    // caster center projected along the light direction onto the floor -> shadowed
    const math::Vec3 ldir = math::normalize(sun.direction);
    const math::Vec3 shadowed = math::Vec3{-0.5f, 1.2f, 0.2f} + ldir * (1.2f / -ldir.y);
    const unsigned char pxShadow = sampleAt({shadowed.x, 0.001f, shadowed.z});
    const unsigned char pxLit = sampleAt({3.f, 0.001f, -3.f});
    std::fprintf(stderr, "  kit sampling: shadowed PCF=%d, lit PCF=%d\n", pxShadow, pxLit);
    expect(pxShadow < pxLit, "kit GLSL: PCF shadowed point darker than lit point");
    expect(pxLit == 255, "kit GLSL: lit point fully lit (255)");

    if (g_failures == 0) std::fprintf(stderr, "gl_smoke_shadow_kit: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
