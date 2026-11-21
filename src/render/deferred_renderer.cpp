#include "fuhgl/render/deferred_renderer.hpp"
#include "fuhgl/render/mesh.hpp"

#include "fuhgl/gl/state.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace fuhgl::render {
namespace {

constexpr const char* kFsVert = R"GLSL(#version 330 core
out vec2 vUV;
void main() {
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;
    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)GLSL";

constexpr const char* kSsaoFragHead = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uPosition; // world pos, alpha = geometry flag
uniform sampler2D uNormal;
uniform mat4 uViewProj;
uniform mat4 uView;
uniform float uRadius;
)GLSL";

constexpr const char* kSsaoFragTail = R"GLSL(
void main() {
    vec4 pos = texture(uPosition, vUV);
    if (pos.a < 0.5) { FragColor = vec4(1.0); return; } // sky: no AO
    vec3 worldPos = pos.rgb;
    vec3 N = normalize(texture(uNormal, vUV).rgb * 2.0 - 1.0);
    vec3 up = abs(N.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);
    float fragDepth = -((uView * vec4(worldPos, 1.0)).z);
    float occ = 0.0;
    for (int i = 0; i < SAMPLES; ++i) {
        vec3 s = worldPos + (T * KERNEL[i].x + B * KERNEL[i].y + N * KERNEL[i].z) * uRadius;
        vec4 clip = uViewProj * vec4(s, 1.0);
        if (clip.w <= 0.0) continue;
        vec2 suv = clip.xy / clip.w * 0.5 + 0.5;
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) continue;
        vec4 sp = texture(uPosition, suv);
        if (sp.a < 0.5) continue;
        float sd = -((uView * vec4(sp.rgb, 1.0)).z);      // stored geometry depth
        float pd = -((uView * vec4(s, 1.0)).z);           // sample depth
        if (sd < pd - 0.02 && abs(sd - fragDepth) < uRadius * 3.0) occ += 1.0;
    }
    FragColor = vec4(vec3(1.0 - occ / float(SAMPLES)), 1.0);
}
)GLSL";

constexpr const char* kBlurFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform vec2 uDir; // (texel, 0) or (0, texel)
void main() {
    vec3 acc = texture(uTex, vUV).rgb * 0.227027;
    vec2 o1 = uDir * 1.3846153846, o2 = uDir * 3.2307692308;
    acc += (texture(uTex, vUV + o1).rgb + texture(uTex, vUV - o1).rgb) * 0.3162162162;
    acc += (texture(uTex, vUV + o2).rgb + texture(uTex, vUV - o2).rgb) * 0.0702702703;
    FragColor = vec4(acc, 1.0);
}
)GLSL";

constexpr const char* kBrightFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uHdr;
uniform float uThreshold;
void main() {
    vec3 c = texture(uHdr, vUV).rgb;
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    FragColor = vec4(c * smoothstep(uThreshold, uThreshold + 0.5, l), 1.0);
}
)GLSL";

constexpr const char* kRaysFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uHdr;
uniform vec2 uSunUV;
uniform float uDensity;
uniform float uDecay;
uniform float uWeight;
void main() {
    vec2 delta = (vUV - uSunUV) * uDensity / 48.0;
    vec2 uv = vUV;
    float illum = 0.0;
    float w = 1.0;
    for (int i = 0; i < 48; ++i) {
        uv -= delta;
        vec3 c = texture(uHdr, clamp(uv, 0.001, 0.999)).rgb;
        float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
        illum += clamp(l - 0.7, 0.0, 1.0) * w;
        w *= uDecay;
    }
    FragColor = vec4(vec3(illum / 48.0 * uWeight), 1.0);
}
)GLSL";

constexpr const char* kCompositeFrag = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform sampler2D uRays;
uniform float uExposure;
uniform float uBloomStrength;
uniform float uRayScale;
vec3 aces(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
void main() {
    vec3 c = texture(uScene, vUV).rgb * uExposure
           + texture(uBloom, vUV).rgb * uBloomStrength
           + texture(uRays, vUV).rgb * uRayScale;
    c = aces(c);
    c = pow(c, vec3(1.0 / 2.2));
    FragColor = vec4(c, 1.0);
}
)GLSL";

float lcg(unsigned& state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
}

std::string ssaoKernelGlsl(int samples) {
    unsigned state = 0x12345678u;
    std::string k = "const int SAMPLES = " + std::to_string(samples) + ";\n";
    k += "const vec3 KERNEL[" + std::to_string(samples) + "] = vec3[](";
    for (int i = 0; i < samples; ++i) {
        const float x = lcg(state) * 2.f - 1.f;
        const float y = lcg(state) * 2.f - 1.f;
        const float z = lcg(state);
        const float len = std::sqrt(x * x + y * y + z * z) + 1e-6f;
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float scale = 0.1f + 0.9f * t * t; // concentrate samples near the fragment
        char buf[96];
        std::snprintf(buf, sizeof buf, "%svec3(%.4ff, %.4ff, %.4ff)", i ? ", " : "",
                      x / len * scale, y / len * scale, z / len * scale);
        k += buf;
    }
    k += ");\n";
    return k;
}

bool compileFs(Material& mat, const std::string& fragSrc, const char* what) {
    if (!mat.create()) return false;
    Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
    if (!vs.compile(kFsVert)) { std::fprintf(stderr, "DeferredRenderer: %s VS: %s\n", what, vs.infoLog().c_str()); return false; }
    if (!fs.compile(fragSrc.c_str())) { std::fprintf(stderr, "DeferredRenderer: %s FS: %s\n", what, fs.infoLog().c_str()); return false; }
    mat.program().attach(vs);
    mat.program().attach(fs);
    if (!mat.program().link()) { std::fprintf(stderr, "DeferredRenderer: %s link: %s\n", what, mat.program().infoLog().c_str()); return false; }
    return true;
}

void makeTargetTex(Texture& tex, gl::GLenum internal, int w, int h) {
    tex = Texture(Texture::Target::Tex2D);
    tex.bind();
    tex.texStorage2D(1, internal, w, h);
    tex.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR);
    tex.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
    tex.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    tex.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
}

bool makeFbo(Framebuffer& fbo, Texture& tex, gl::GLenum internal, int w, int h, const char* what) {
    makeTargetTex(tex, internal, w, h);
    fbo.create();
    fbo.setSize(w, h);
    fbo.bind();
    fbo.attachTexture2D(0, tex);
    if (!fbo.isComplete()) {
        std::fprintf(stderr, "DeferredRenderer: %s FBO incomplete (0x%x)\n", what, fbo.status());
        Framebuffer::unbind();
        return false;
    }
    Framebuffer::unbind();
    return true;
}

} // namespace

struct DeferredRenderer::Impl {};

DeferredRenderer::DeferredRenderer() : impl_(std::make_unique<Impl>()) {}
DeferredRenderer::~DeferredRenderer() = default;
DeferredRenderer::DeferredRenderer(DeferredRenderer&&) noexcept = default;
DeferredRenderer& DeferredRenderer::operator=(DeferredRenderer&&) noexcept = default;

math::Vec2 sunScreenUV(const Camera& camera, const math::Vec3& sunDir, int width, int height) {
    if (width <= 0 || height <= 0) return {-1.f, -1.f};
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const math::Mat4 vp = camera.viewProj(aspect);
    const math::Vec3 p = camera.eye() + math::normalize(sunDir) * 1000.f;
    const math::Vec4 c = vp * math::Vec4(p, 1.f);
    if (c.w <= 0.f) return {-1.f, -1.f}; // behind the camera
    return {c.x / c.w * 0.5f + 0.5f, c.y / c.w * 0.5f + 0.5f};
}

bool DeferredRenderer::create(int width, int height, int msaaSamples) {
    width_ = width;
    height_ = height;
    msaa_ = msaaSamples;
    if (!pipeline_.create(width, height, msaaSamples)) {
        std::fprintf(stderr, "DeferredRenderer: pipeline create failed\n");
        return false;
    }
    if (!createPost(width, height)) return false;
    if (shadows_ && !shadowMap_.create(shadowSize_)) {
        std::fprintf(stderr, "DeferredRenderer: shadow map create failed\n");
        return false;
    }
    return true;
}

bool DeferredRenderer::createPost(int w, int h) {
    if (!makeFbo(ssaoFbo_, texSsao_, gl::GL_RGBA8, w, h, "ssao")) return false;
    if (!makeFbo(blurFbo_, texBlur_, gl::GL_RGBA8, w, h, "blur")) return false;
    if (!makeFbo(hdrFbo_, texHdr_, gl::GL_RGBA16F, w, h, "hdr")) return false;
    if (!makeFbo(bloomFboA_, texBloomA_, gl::GL_RGBA16F, w, h, "bloomA")) return false;
    if (!makeFbo(bloomFboB_, texBloomB_, gl::GL_RGBA16F, w, h, "bloomB")) return false;
    if (!makeFbo(raysFbo_, texRays_, gl::GL_RGBA16F, w, h, "rays")) return false;

    const std::string ssaoSrc = std::string(kSsaoFragHead) + ssaoKernelGlsl(ssaoSamples) + kSsaoFragTail;
    if (!compileFs(ssaoMat_, ssaoSrc, "ssao")) return false;
    if (!compileFs(blurMat_, kBlurFrag, "blur")) return false;
    if (!compileFs(brightMat_, kBrightFrag, "bright")) return false;
    if (!compileFs(raysMat_, kRaysFrag, "rays")) return false;
    if (!compileFs(compositeMat_, kCompositeFrag, "composite")) return false;

    if (!fsVao_.create()) return false;
    fsVao_.bind();
    return true;
}

void DeferredRenderer::resize(int width, int height) {
    if (width == width_ && height == height_) return;
    *this = DeferredRenderer();
    create(width, height, msaa_);
}

void DeferredRenderer::enableCsmShadows(int mapSize, int cascades) {
    shadowSize_ = mapSize;
    csmEnabled_ = true;
    if (gl::isInitialized() && csm_.cascadeCount() <= 0 && !csm_.create(mapSize, cascades)) {
        std::fprintf(stderr, "DeferredRenderer: CSM create failed\n");
        csmEnabled_ = false;
    }
}

void DeferredRenderer::enableShadows(int mapSize) {
    shadowSize_ = mapSize;
    shadows_ = true;
    if (gl::isInitialized() && shadowMap_.size() <= 0 && !shadowMap_.create(mapSize)) {
        std::fprintf(stderr, "DeferredRenderer: shadow map create failed\n");
        shadows_ = false;
    }
}

void DeferredRenderer::beginPass(Framebuffer& fbo) {
    fbo.bind();
    const gl::GLenum buf[1] = {gl::GL_COLOR_ATTACHMENT0};
    gl::drawBuffers(1, buf);
    gl::viewport(0, 0, width_, height_);
    gl::disable(gl::Cap::DepthTest);
    gl::disable(gl::Cap::Blend);
}

void DeferredRenderer::endPass() { Framebuffer::unbind(); }

void DeferredRenderer::drawFs(Material& mat) {
    mat.bind();
    fsVao_.bind();
    gl::drawArrays(gl::Primitive::Triangles, 0, 3);
}

void DeferredRenderer::runSSAO(const SceneView& scene) {
    ssaoMat_.setTexture(0, pipeline_.texPosition());
    ssaoMat_.setTexture(1, pipeline_.texNormal());
    beginPass(ssaoFbo_);
    ssaoMat_.bind();
    {
        Program& p = ssaoMat_.program();
        p.setUniform("uViewProj", scene.camera->viewProj(scene.aspect));
        p.setUniform("uView", scene.camera->view());
        p.setUniform("uRadius", ssaoRadius);
        p.setUniform(p.uniformLocation("uPosition"), 0);
        p.setUniform(p.uniformLocation("uNormal"), 1);
    }
    fsVao_.bind();
    gl::drawArrays(gl::Primitive::Triangles, 0, 3);
    endPass();
    // separable blur: H into blurFbo, V back into ssaoFbo
    const float tx = 1.f / static_cast<float>(width_), ty = 1.f / static_cast<float>(height_);
    blurMat_.setTexture(0, texSsao_);
    beginPass(blurFbo_);
    blurMat_.bind();
    {
        Program& p = blurMat_.program();
        p.setUniform("uDir", math::Vec2(tx, 0.f));
        p.setUniform(p.uniformLocation("uTex"), 0);
    }
    fsVao_.bind();
    gl::drawArrays(gl::Primitive::Triangles, 0, 3);
    endPass();
    blurMat_.setTexture(0, texBlur_);
    beginPass(ssaoFbo_);
    blurMat_.bind();
    {
        Program& p = blurMat_.program();
        p.setUniform("uDir", math::Vec2(0.f, ty));
        p.setUniform(p.uniformLocation("uTex"), 0);
    }
    fsVao_.bind();
    gl::drawArrays(gl::Primitive::Triangles, 0, 3);
    endPass();
    pipeline_.aoTexture = &texSsao_;
    pipeline_.aoStrength = ssaoStrength;
}

void DeferredRenderer::runBloom() {
    brightMat_.setTexture(0, texHdr_);
    beginPass(bloomFboA_);
    brightMat_.bind();
    {
        Program& p = brightMat_.program();
        p.setUniform("uThreshold", bloomThreshold);
        p.setUniform(p.uniformLocation("uHdr"), 0);
    }
    fsVao_.bind();
    gl::drawArrays(gl::Primitive::Triangles, 0, 3);
    endPass();

    const float tx = 1.f / static_cast<float>(width_), ty = 1.f / static_cast<float>(height_);
    for (int i = 0; i < bloomBlurPasses; ++i) {
        blurMat_.setTexture(0, texBloomA_);
        beginPass(bloomFboB_);
        blurMat_.bind();
        {
            Program& p = blurMat_.program();
            p.setUniform("uDir", math::Vec2(tx, 0.f));
            p.setUniform(p.uniformLocation("uTex"), 0);
        }
        fsVao_.bind();
        gl::drawArrays(gl::Primitive::Triangles, 0, 3);
        endPass();
        blurMat_.setTexture(0, texBloomB_);
        beginPass(bloomFboA_);
        blurMat_.bind();
        {
            Program& p = blurMat_.program();
            p.setUniform("uDir", math::Vec2(0.f, ty));
            p.setUniform(p.uniformLocation("uTex"), 0);
        }
        fsVao_.bind();
        gl::drawArrays(gl::Primitive::Triangles, 0, 3);
        endPass();
    }
}

void DeferredRenderer::runRays(const SceneView& scene) {
    const math::Vec2 suv = sunScreenUV(*scene.camera, scene.sunDirection, width_, height_);
    const bool visible = suv.x >= 0.f;
    raysMat_.setTexture(0, texHdr_);
    beginPass(raysFbo_);
    raysMat_.bind();
    {
        Program& p = raysMat_.program();
        p.setUniform("uSunUV", visible ? suv : math::Vec2(0.5f, 1.5f));
        p.setUniform("uDensity", rayDensity);
        p.setUniform("uDecay", rayDecay);
        p.setUniform("uWeight", (visible && godRaysEnabled) ? rayWeight : 0.f);
        p.setUniform(p.uniformLocation("uHdr"), 0);
    }
    fsVao_.bind();
    gl::drawArrays(gl::Primitive::Triangles, 0, 3);
    endPass();
}

void DeferredRenderer::runComposite() {
    compositeMat_.setTexture(0, texHdr_);
    compositeMat_.setTexture(1, texBloomA_);
    compositeMat_.setTexture(2, texRays_);
    beginPass(pipeline_.output());
    compositeMat_.bind();
    {
        Program& p = compositeMat_.program();
        p.setUniform(p.uniformLocation("uScene"), 0);
        p.setUniform(p.uniformLocation("uBloom"), 1);
        p.setUniform(p.uniformLocation("uRays"), 2);
        p.setUniform("uExposure", exposure);
        p.setUniform("uBloomStrength", bloomStrength);
        p.setUniform("uRayScale", godRaysEnabled ? 1.f : 0.f);
    }
    fsVao_.bind();
    gl::drawArrays(gl::Primitive::Triangles, 0, 3);
    endPass();
}

void DeferredRenderer::render(const SceneView& scene) {
    if (!scene.camera || width_ <= 0) return;
    SceneView s = scene;

    // shadow depth pass: CSM (F30, first directional light) or single map (first directional/spot)
    if (csmEnabled_ && csm_.cascadeCount() > 0) {
        for (size_t i = 0; i < s.lights.size(); ++i) {
            if (s.lights[i].type == LightType::Directional && s.camera) {
                csm_.render(s, *s.camera, s.lights[i], shadowCenter_, shadowRadius_, shadowZNear_, shadowZFar_);
                s.cascades = &csm_;
                s.shadowLightIndex = static_cast<int>(i);
                s.shadowStrength = shadowStrength_;
                break;
            }
        }
    } else if (shadows_ && shadowMap_.size() > 0) {
        for (size_t i = 0; i < s.lights.size(); ++i) {
            if (s.lights[i].type == LightType::Directional || s.lights[i].type == LightType::Spot) {
                s.lightVP = lightViewProj(s.lights[i], shadowCenter_, shadowRadius_);
                shadowMap_.render(s, s.lightVP);
                s.shadowMap = &shadowMap_;
                s.shadowLightIndex = static_cast<int>(i);
                s.shadowStrength = shadowStrength_;
                break;
            }
        }
    }
    // sky sun disk + god-ray source from the first directional light
    for (const Light& l : s.lights) {
        if (l.type == LightType::Directional) {
            s.sunDirection = -math::normalize(l.direction);
            s.sunColor = l.color * (l.intensity * 4.f);
            break;
        }
    }

    pipeline_.renderGBuffer(s);
    if (ssaoEnabled)
        runSSAO(s);
    else
        pipeline_.aoTexture = nullptr;
    pipeline_.renderLighting(s, &hdrFbo_);
    runBloom();
    runRays(s);
    runComposite();
}

} // namespace fuhgl::render
