#include "fuhgl/render/deferred.hpp"
#include "fuhgl/render/lod.hpp"
#include "fuhgl/render/shadow.hpp"
#include "fuhgl/render/environment.hpp"
#include "lighting_glsl.hpp"
#include "pipeline_internal.hpp"

#include "fuhgl/gl/state.hpp"

#include <cstdio>

namespace fuhgl::render {
namespace {

// --- G-buffer geometry pass ----------------------------------------------------
constexpr const char* kGBufferVert = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;
layout(location = 4) in mat4 aInstanceModel; // F24 instancing (unused when uInstanced == 0)
uniform mat4 uModel;
uniform mat4 uViewProj;
uniform int uInstanced;
out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec2 vUV;
void main() {
    mat4 model = (uInstanced == 1) ? (uModel * aInstanceModel) : uModel;
    vec4 wp = model * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vNormal = mat3(model) * aNormal;
    vColor = aColor;
    vUV = aUV;
    gl_Position = uViewProj * wp;
}
)GLSL";

constexpr const char* kGBufferFrag = R"GLSL(#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vUV;
uniform vec3 uAlbedo;
uniform vec3 uEmissive;
uniform int uUseTex;
uniform sampler2D uMatTex;
uniform vec3 uCamPos;
uniform int uUseNormalMap;
uniform sampler2D uNormalTex;
uniform int uUseHeightMap;
uniform sampler2D uHeightTex;
uniform float uParallaxScale;
uniform int uParallaxLayers;
uniform float uMetallic;   // F28 PBR
uniform float uRoughness;
uniform float uAo;
layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oPosition;
layout(location = 3) out vec4 oEmissive;
layout(location = 4) out vec4 oPbr;
void main() {
    vec2 uv = vUV;
    vec3 N = normalize(vNormal);
    // tangent frame from screen-space derivatives (no tangent attribute needed)
    vec3 dp1 = dFdx(vWorldPos);
    vec3 dp2 = dFdy(vWorldPos);
    vec2 duv1 = dFdx(vUV);
    vec2 duv2 = dFdy(vUV);
    vec3 T = normalize(dp1 * duv2.y - dp2 * duv1.y);
    vec3 B = normalize(-dp1 * duv2.x + dp2 * duv1.x);
    if (uUseHeightMap == 1) {
        // parallax occlusion mapping: march the view ray through the height layers
        vec3 V = normalize(uCamPos - vWorldPos);
        vec3 viewTS = vec3(dot(T, V), dot(B, V), dot(N, V));
        vec2 p = viewTS.xy / max(viewTS.z, 0.05) * uParallaxScale;
        float layers = float(max(uParallaxLayers, 1));
        vec2 duv = p / layers;
        float layerDepth = 0.0;
        vec2 curUV = uv;
        float h = texture(uHeightTex, curUV).r;
        for (int i = 0; i < 64 && float(i) < layers; ++i) {
            if (layerDepth >= h) break;
            curUV -= duv;
            layerDepth += 1.0 / layers;
            h = texture(uHeightTex, curUV).r;
        }
        uv = curUV;
    }
    vec3 albedo = uAlbedo * vColor;
    if (uUseTex == 1) albedo *= texture(uMatTex, uv).rgb;
    if (uUseNormalMap == 1) {
        vec3 nTS = texture(uNormalTex, uv).rgb * 2.0 - 1.0;
        N = normalize(T * nTS.x + B * nTS.y + N * nTS.z);
    }
    oAlbedo = vec4(albedo, 1.0);
    oNormal = vec4(N * 0.5 + 0.5, 1.0);
    oPosition = vec4(vWorldPos, 1.0); // alpha = 1 marks "geometry present"
    oEmissive = vec4(uEmissive, 1.0);
    oPbr = vec4(uMetallic, uRoughness, uAo, 1.0);
}
)GLSL";

// --- fullscreen lighting pass ----------------------------------------------------
constexpr const char* kLightVert = R"GLSL(#version 330 core
out vec2 vUV;
void main() {
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;
    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)GLSL";

constexpr const char* kLightFragHead = R"GLSL(#version 330 core
in vec2 vUV;
uniform sampler2D uAlbedo;
uniform sampler2D uNormal;
uniform sampler2D uPosition;
uniform sampler2D uEmissive;
uniform vec3 uSkyTop;
uniform vec3 uSkyBottom;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform sampler2D uAO;
uniform int uUseAO;
uniform float uAOStrength;
uniform sampler2D uPbr; // F28: metallic/roughness/ao G-buffer texture (unit kTextureUnitGBufferPbr)
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunCos;
uniform mat4 uInvViewProj;
)GLSL";

constexpr const char* kLightFragTail = R"GLSL(
out vec4 FragColor;
void main() {
    vec4 pos = texture(uPosition, vUV);
    if (pos.a < 0.5) {
        // background: sky gradient + HDR sun disk (god-ray/bloom source)
        vec4 fp = uInvViewProj * vec4(vUV * 2.0 - 1.0, 1.0, 1.0); // far-plane point (needs w division!)
        vec3 ray = normalize(fp.xyz / fp.w - uCamPos);
        vec3 sky = mix(uSkyBottom, uSkyTop, clamp(vUV.y, 0.0, 1.0));
        float s = dot(ray, uSunDir);
        sky += uSunColor * smoothstep(uSunCos, 1.0, s);
        FragColor = vec4(sky, 1.0);
        return;
    }
    vec3 worldPos = pos.rgb;
    vec3 N = normalize(texture(uNormal, vUV).rgb * 2.0 - 1.0);
    vec3 albedo = texture(uAlbedo, vUV).rgb;
    vec3 emissive = texture(uEmissive, vUV).rgb;
    vec4 pbr = texture(uPbr, vUV);
    vec3 V = normalize(uCamPos - worldPos);
    vec3 lit = shade(albedo, pbr.r, pbr.g, pbr.b, N, worldPos, V) + emissive;
    if (uUseAO == 1) lit *= mix(1.0, texture(uAO, vUV).r, clamp(uAOStrength, 0.0, 2.0));
    if (uFogDensity > 0.0) {
        float d = length(worldPos - uCamPos);
        lit = mix(lit, uFogColor, 1.0 - exp(-uFogDensity * d));
    }
    FragColor = vec4(lit, 1.0);
}
)GLSL";

void makeTarget(Texture& tex, gl::GLenum internal, int w, int h, int msaa) {
    if (msaa > 0) {
        tex = Texture(Texture::Target::Tex2DMultisample);
        tex.bind();
        tex.storageMultisample2D(msaa, internal, w, h);
    } else {
        tex = Texture(Texture::Target::Tex2D);
        tex.bind();
        tex.texStorage2D(1, internal, w, h);
        tex.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
        tex.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
        tex.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
        tex.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    }
}

} // namespace

struct DeferredPipeline::Impl {};

DeferredPipeline::DeferredPipeline() : impl_(std::make_unique<Impl>()) {}
DeferredPipeline::~DeferredPipeline() = default;
DeferredPipeline::DeferredPipeline(DeferredPipeline&&) noexcept = default;
DeferredPipeline& DeferredPipeline::operator=(DeferredPipeline&&) noexcept = default;

bool DeferredPipeline::create(int width, int height, int msaaSamples) {
    width_ = width;
    height_ = height;
    msaa_ = msaaSamples;

    // G-buffer textures (multisampled when requested)
    makeTarget(texAlbedo_, gl::GL_RGBA8, width, height, msaa_);
    makeTarget(texNormal_, gl::GL_RGBA8, width, height, msaa_);
    makeTarget(texPosition_, gl::GL_RGBA16F, width, height, msaa_);
    makeTarget(texEmissive_, gl::GL_RGBA8, width, height, msaa_);
    makeTarget(texPbr_, gl::GL_RGBA8, width, height, msaa_); // F28: metallic/roughness/ao

    gbuffer_.create();
    gbuffer_.setSize(width, height);
    gbuffer_.bind();
    gbuffer_.attachTexture2D(0, texAlbedo_);
    gbuffer_.attachTexture2D(1, texNormal_);
    gbuffer_.attachTexture2D(2, texPosition_);
    gbuffer_.attachTexture2D(3, texEmissive_);
    gbuffer_.attachTexture2D(4, texPbr_);
    if (msaa_ > 0) {
        depthMsaa_.create();
        depthMsaa_.bind();
        depthMsaa_.storageMultisample(msaa_, gl::GL_DEPTH24_STENCIL8, width, height);
        gbuffer_.attachRenderbuffer(Framebuffer::Attachment::Depth, depthMsaa_);
    } else {
        depth_.create();
        depth_.bind();
        depth_.storage(gl::GL_DEPTH_COMPONENT24, width, height);
        gbuffer_.attachRenderbuffer(Framebuffer::Attachment::Depth, depth_);
    }
    if (!gbuffer_.isComplete()) {
        std::fprintf(stderr, "DeferredPipeline: G-buffer incomplete (0x%x)\n", gbuffer_.status());
        return false;
    }

    // resolve target (only for MSAA)
    if (msaa_ > 0) {
        makeTarget(texResolveAlbedo_, gl::GL_RGBA8, width, height, 0);
        makeTarget(texResolveNormal_, gl::GL_RGBA8, width, height, 0);
        makeTarget(texResolvePosition_, gl::GL_RGBA16F, width, height, 0);
        makeTarget(texResolveEmissive_, gl::GL_RGBA8, width, height, 0);
        makeTarget(texResolvePbr_, gl::GL_RGBA8, width, height, 0);
        resolve_.create();
        resolve_.setSize(width, height);
        resolve_.bind();
        resolve_.attachTexture2D(0, texResolveAlbedo_);
        resolve_.attachTexture2D(1, texResolveNormal_);
        resolve_.attachTexture2D(2, texResolvePosition_);
        resolve_.attachTexture2D(3, texResolveEmissive_);
        resolve_.attachTexture2D(4, texResolvePbr_);
        if (!resolve_.isComplete()) {
            std::fprintf(stderr, "DeferredPipeline: resolve FBO incomplete (0x%x)\n", resolve_.status());
            return false;
        }
    }

    // lighting output
    makeTarget(texOutput_, gl::GL_RGBA8, width, height, 0);
    output_.create();
    output_.setSize(width, height);
    output_.bind();
    output_.attachTexture2D(0, texOutput_);
    if (!output_.isComplete()) {
        std::fprintf(stderr, "DeferredPipeline: output FBO incomplete (0x%x)\n", output_.status());
        return false;
    }
    Framebuffer::unbind();

    // default G-buffer material
    if (!defaultMaterial_.create()) { std::fprintf(stderr, "DeferredPipeline: default material create failed\n"); return false; }
    {
        Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
        if (!vs.compile(kGBufferVert)) { std::fprintf(stderr, "DeferredPipeline: gbuf VS: %s\n", vs.infoLog().c_str()); return false; }
        if (!fs.compile(kGBufferFrag)) { std::fprintf(stderr, "DeferredPipeline: gbuf FS: %s\n", fs.infoLog().c_str()); return false; }
        defaultMaterial_.program().attach(vs);
        defaultMaterial_.program().attach(fs);
        if (!defaultMaterial_.program().link()) {
            std::fprintf(stderr, "DeferredPipeline: gbuf link: %s\n", defaultMaterial_.program().infoLog().c_str());
            return false;
        }
        defaultMaterial_.program().bindUniformBlock("LightBlock", 0); // not used by gbuf pass, harmless
    }
    // lighting material (fullscreen triangle)
    if (!lightingMaterial_.create()) return false;
    {
        Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
        if (!vs.compile(kLightVert)) { std::fprintf(stderr, "DeferredPipeline: light VS: %s\n", vs.infoLog().c_str()); return false; }
        const std::string fragSrc = std::string(kLightFragHead) + kLightingUniforms + kShadowUniforms + kShadowFunc +
                                    kEnvUniforms + kPbrFuncs + kLightingLoop + kLightFragTail;
        if (!fs.compile(fragSrc.c_str())) { std::fprintf(stderr, "DeferredPipeline: light FS: %s\n", fs.infoLog().c_str()); return false; }
        lightingMaterial_.program().attach(vs);
        lightingMaterial_.program().attach(fs);
        if (!lightingMaterial_.program().link()) {
            std::fprintf(stderr, "DeferredPipeline: light link: %s\n", lightingMaterial_.program().infoLog().c_str());
            return false;
        }
        lightingMaterial_.program().bindUniformBlock("LightBlock", 0);
    }
    lightingMaterial_.setTexture(0, texAlbedo_);
    lightingMaterial_.setTexture(1, texNormal_);
    lightingMaterial_.setTexture(2, texPosition_);
    lightingMaterial_.setTexture(3, texEmissive_);
    lightingMaterial_.setTexture(kTextureUnitGBufferPbr, texPbr_);

    if (!lightsUbo_.create()) return false;
    lightsUbo_.bindBase(0);
    // 1x1 dummy depth texture: keeps the uShadowMap sampler bound even when shadows are off
    dummyShadowTex_.bind();
    dummyShadowTex_.texStorage2D(1, gl::GL_DEPTH_COMPONENT24, 1, 1);
    dummyShadowTex_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
    dummyShadowTex_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
    dummyShadowTex_.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    dummyShadowTex_.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    // F30: 1x1x1 depth array so the sampler2DArray stays valid when CSM is off
    dummyShadowArrayTex_.bind();
    dummyShadowArrayTex_.texStorage3D(1, gl::GL_DEPTH_COMPONENT24, 1, 1, 1);
    dummyShadowArrayTex_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
    dummyShadowArrayTex_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
    dummyShadowArrayTex_.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    dummyShadowArrayTex_.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    fullscreenVao_.create();
    fullscreenVao_.bind();
    return true;
}

void DeferredPipeline::resize(int width, int height) {
    if (width == width_ && height == height_) return;
    // simplest correct approach: recreate everything
    *this = DeferredPipeline();
    create(width, height, msaa_);
}

void DeferredPipeline::renderGBuffer(const SceneView& scene) {
    if (!scene.camera || width_ <= 0) return;
    auto& f = gl::functions();

    // The pipeline owns the GL state it needs: depth test + CCW back-face culling.
    gl::enable(gl::Cap::DepthTest);
    gl::depthFunc(gl::CompareFunc::Less);
    gl::depthMask(true);
    gl::enable(gl::Cap::CullFace);
    gl::cullFace(gl::Face::Back);
    gl::frontFace(true);

    // upload lights (count only needed by the lighting pass below)
    LightBlock block{};
    packLightBlock(scene.lights, block);
    lightsUbo_.upload(block);
    lightsUbo_.bindBase(0);

    // --- pass 1: G-buffer ---
    gbuffer_.bind();
    const gl::GLenum drawBufs[5] = {gl::GL_COLOR_ATTACHMENT0, gl::GL_COLOR_ATTACHMENT1, gl::GL_COLOR_ATTACHMENT2,
                                    gl::GL_COLOR_ATTACHMENT3, gl::GL_COLOR_ATTACHMENT4};
    gl::drawBuffers(5, drawBufs);
    gl::viewport(0, 0, width_, height_);
    const float clearZero[4] = {0.f, 0.f, 0.f, 0.f};
    const float clearAlbedo[4] = {scene.clearColor.x, scene.clearColor.y, scene.clearColor.z, 1.f};
    gl::clearBufferfv(gl::GL_COLOR, 0, clearAlbedo);
    gl::clearBufferfv(gl::GL_COLOR, 1, clearZero);
    gl::clearBufferfv(gl::GL_COLOR, 2, clearZero); // position alpha 0 = background
    gl::clearBufferfv(gl::GL_COLOR, 3, clearZero);
    gl::clearBufferfv(gl::GL_COLOR, 4, clearZero); // pbr
    gl::clear(gl::ClearBit::Depth);

    const math::Mat4 viewProj = math::jitterViewProj(scene.camera->viewProj(scene.aspect), scene.jitter); // F32
    for (const DrawItem& item : scene.items) {
        int lodLevel = -1;
        const Mesh* mesh = resolveLodMesh(item, scene.camera->eye(), scene.lodEnabled, &lodLevel); // F41
        if (!mesh) continue;
        if (lodLevel >= 0) scene.lastLodLevel = lodLevel;
        detail::bindItem(defaultMaterial_, item, mesh, item.model, viewProj);
        Program& prog = const_cast<Program&>(
            (item.material && item.material->builtinShader) ? defaultMaterial_.program()
                                                            : (item.material ? *item.material : defaultMaterial_).program());
        prog.setUniform("uCamPos", scene.camera->eye());
        mesh->draw();
    }

    // --- MSAA resolve ---
    if (msaa_ > 0) {
        for (int i = 0; i < 5; ++i) {
            const gl::GLenum att = gl::GL_COLOR_ATTACHMENT0 + i;
            f.glReadBuffer(att);
            const gl::GLenum dst[1] = {att};
            f.glBindFramebuffer(gl::GL_READ_FRAMEBUFFER, gbuffer_.id());
            f.glBindFramebuffer(gl::GL_DRAW_FRAMEBUFFER, resolve_.id());
            gl::drawBuffers(1, dst);
            f.glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, gl::GL_COLOR_BUFFER_BIT, gl::GL_NEAREST);
        }
        f.glBindFramebuffer(gl::GL_READ_FRAMEBUFFER, 0);
        f.glBindFramebuffer(gl::GL_DRAW_FRAMEBUFFER, 0);
        // lighting samples the resolved textures
        lightingMaterial_.setTexture(0, texResolveAlbedo_);
        lightingMaterial_.setTexture(1, texResolveNormal_);
        lightingMaterial_.setTexture(2, texResolvePosition_);
        lightingMaterial_.setTexture(3, texResolveEmissive_);
        lightingMaterial_.setTexture(kTextureUnitGBufferPbr, texResolvePbr_);
    }
}

void DeferredPipeline::renderLighting(const SceneView& scene, Framebuffer* target) {
    if (!scene.camera || width_ <= 0) return;

    // --- pass 2: lighting (fullscreen) ---
    Framebuffer& out = target ? *target : output_;
    out.bind();
    const gl::GLenum outBuf[1] = {gl::GL_COLOR_ATTACHMENT0};
    gl::drawBuffers(1, outBuf);
    gl::viewport(0, 0, width_, height_);
    const float outClear[4] = {scene.clearColor.x, scene.clearColor.y, scene.clearColor.z, 1.f};
    gl::clearBufferfv(gl::GL_COLOR, 0, outClear);

    LightBlock block{};
    const int n = packLightBlock(scene.lights, block);
    lightsUbo_.upload(block);
    lightsUbo_.bindBase(0);

    lightingMaterial_.bind(); // binds program + gbuffer textures (units 0..3)
    Program& lp = lightingMaterial_.program();
    lp.setUniform(lp.uniformLocation("uAlbedo"), 0);
    lp.setUniform(lp.uniformLocation("uNormal"), 1);
    lp.setUniform(lp.uniformLocation("uPosition"), 2);
    lp.setUniform(lp.uniformLocation("uEmissive"), 3);
    lp.setUniform(lp.uniformLocation("uPbr"), kTextureUnitGBufferPbr);
    // F29: IBL environment (irradiance/prefilter/BRDF LUT on units 7/8/9)
    lp.setUniform(lp.uniformLocation("uIrradianceMap"), kTextureUnitIrradiance);
    lp.setUniform(lp.uniformLocation("uPrefilterMap"), kTextureUnitPrefilter);
    lp.setUniform(lp.uniformLocation("uBrdfLut"), kTextureUnitBrdfLut);
    if (scene.env && scene.env->irradianceMap().id()) {
        gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitIrradiance);
        gl::functions().glBindTexture(gl::GL_TEXTURE_CUBE_MAP, scene.env->irradianceMap().id());
        gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitPrefilter);
        gl::functions().glBindTexture(gl::GL_TEXTURE_CUBE_MAP, scene.env->prefilterMap().id());
        gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitBrdfLut);
        gl::functions().glBindTexture(gl::GL_TEXTURE_2D, scene.env->brdfLut().id());
        lp.setUniform(lp.uniformLocation("uEnvEnabled"), 1);
        lp.setUniform("uPrefilterMaxLod", static_cast<float>(scene.env->prefilterMaxLod()));
        lp.setUniform("uEnvIntensity", scene.env->intensity);
    } else {
        lp.setUniform(lp.uniformLocation("uEnvEnabled"), 0);
    }
    lp.setUniform("uCamPos", scene.camera->eye());
    lp.setUniform("uAmbient", scene.ambient);
    lp.setUniform(lp.uniformLocation("uLightCount"), n);
    lp.setUniform("uSkyTop", skyTop);
    lp.setUniform("uSkyBottom", skyBottom);
    // shadow map on unit kTextureUnitShadowMap (dummy 1x1 when disabled);
    // F30: a CascadeShadowMap binds a GL_TEXTURE_2D_ARRAY on the same unit instead
    const bool csm = scene.cascades && scene.cascades->cascadeCount() > 0;
    // unit kTextureUnitShadowMap: 2D depth map (or dummy); unit kTextureUnitShadowMapArray:
    // CSM depth array (or dummy array) — both sampler types must have a matching texture bound
    gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitShadowMap);
    gl::functions().glBindTexture(gl::GL_TEXTURE_2D,
                                  scene.shadowMap ? scene.shadowMap->textureId() : dummyShadowTex_.id());
    gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitShadowMapArray);
    gl::functions().glBindTexture(gl::GL_TEXTURE_2D_ARRAY,
                                  csm ? scene.cascades->textureId() : dummyShadowArrayTex_.id());
    lp.setUniform("uView", scene.camera->view());
    lp.setUniform(lp.uniformLocation("uShadowMapArray"), kTextureUnitShadowMapArray);
    if (csm) {
        lp.setUniform(lp.uniformLocation("uShadowMode"), 1);
        lp.setUniform(lp.uniformLocation("uCascadeCount"), scene.cascades->cascadeCount());
        lp.setUniform(lp.uniformLocation("uCascadeSplits"), scene.cascades->cascadeCount() + 1,
                      scene.cascades->splits().data());
        lp.setUniform(lp.uniformLocation("uLightVPs"), scene.cascades->cascadeCount(),
                      &scene.cascades->cascadeVP(0));
        lp.setUniform(lp.uniformLocation("uShadowEnabled"), 1);
    } else {
        lp.setUniform(lp.uniformLocation("uShadowMode"), 0);
        lp.setUniform(lp.uniformLocation("uShadowEnabled"), scene.shadowMap ? 1 : 0);
        lp.setUniform(lp.uniformLocation("uShadowMap"), kTextureUnitShadowMap);
        lp.setUniform("uLightVP", scene.lightVP);
    }
    lp.setUniform(lp.uniformLocation("uShadowLightIndex"), scene.shadowLightIndex);
    lp.setUniform("uShadowStrength", scene.shadowStrength);
    lp.setUniform("uFogColor", scene.fogColor);
    lp.setUniform("uFogDensity", scene.fogDensity);
    // SSAO map (F17)
    if (aoTexture && aoTexture->id()) {
        gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitSSAO);
        gl::functions().glBindTexture(gl::GL_TEXTURE_2D, aoTexture->id());
        lp.setUniform(lp.uniformLocation("uUseAO"), 1);
        lp.setUniform(lp.uniformLocation("uAO"), kTextureUnitSSAO);
        lp.setUniform("uAOStrength", aoStrength);
    } else {
        lp.setUniform(lp.uniformLocation("uUseAO"), 0);
    }
    // sky sun disk (HDR source for bloom/god rays) + inverse VP for background rays
    lp.setUniform("uSunDir", scene.sunDirection);
    lp.setUniform("uSunColor", scene.sunColor);
    lp.setUniform("uSunCos", scene.sunCos);
    lp.setUniform("uInvViewProj", scene.camera->viewProj(scene.aspect).inverse());
    fullscreenVao_.bind();
    gl::drawArrays(gl::Primitive::Triangles, 0, 3);

    Framebuffer::unbind();
}

void DeferredPipeline::render(const SceneView& scene) {
    renderGBuffer(scene);
    renderLighting(scene);
}

} // namespace fuhgl::render
