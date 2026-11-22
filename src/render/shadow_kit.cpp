#include "fuhgl/render/shadow_kit.hpp"

#include "fuhgl/render/lod.hpp"
#include "fuhgl/render/mesh.hpp"
#include "fuhgl/render/shadow.hpp"
#include "fuhgl/render/shadow_glsl.hpp"

#include "fuhgl/cam/camera.hpp"
#include "fuhgl/gl/state.hpp"

#include <cmath>

#include <cstdio>

namespace fuhgl::render {
namespace {

bool buildDepthMaterial(Material& mat, bool moments) {
    if (!mat.create()) return false;
    Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
    if (!vs.compile(kSkDepthVert)) {
        std::fprintf(stderr, "shadow_kit: depth VS: %s\n", vs.infoLog().c_str());
        return false;
    }
    if (!fs.compile(moments ? kSkMomentsFrag : kSkDepthFrag)) {
        std::fprintf(stderr, "shadow_kit: depth FS: %s\n", fs.infoLog().c_str());
        return false;
    }
    mat.program().attach(vs);
    mat.program().attach(fs);
    if (!mat.program().link()) {
        std::fprintf(stderr, "shadow_kit: depth link: %s\n", mat.program().infoLog().c_str());
        return false;
    }
    return true;
}

void drawSceneDepth(const SceneView& scene, Program& p, const math::Mat4& vp) {
    p.setUniform("uLightVP", vp);
    for (const DrawItem& item : scene.items) {
        const Mesh* mesh = resolveLodMesh(item, scene.camera ? scene.camera->eye() : math::Vec3{0.f, 0.f, 0.f},
                                          scene.lodEnabled); // F41
        if (!mesh) continue;
        p.setUniform("uModel", item.model);
        p.setUniform("uInstanced", mesh->instanceCount() > 0 ? 1 : 0);
        mesh->draw();
    }
}

void configureDepthTexture(Texture& tex, Texture::Target target, gl::GLint internalFormat,
                           int size, int layers) {
    tex.create(target);
    tex.bind();
    if (target == Texture::Target::Tex2DArray)
        tex.texStorage3D(1, internalFormat, size, size, layers);
    else // Tex2D and Cube (glTexStorage2D on a cube target allocates all 6 faces)
        tex.texStorage2D(1, internalFormat, size, size);
    tex.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR); // HW 2x2 + shader PCF/PCSS/VSM
    tex.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
    tex.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    tex.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    if (target == Texture::Target::Cube)
        tex.setParameteri(gl::GL_TEXTURE_WRAP_R, gl::GL_CLAMP_TO_EDGE);
}

void attachDepth(ShadowTargetKind kind, const Texture& depth, const Texture* moments,
                 int layerOrFace) {
    const auto& f = gl::functions();
    if (kind == ShadowTargetKind::Tex2D) {
        f.glFramebufferTexture2D(gl::GL_FRAMEBUFFER, gl::GL_DEPTH_ATTACHMENT, gl::GL_TEXTURE_2D, depth.id(), 0);
        if (moments)
            f.glFramebufferTexture2D(gl::GL_FRAMEBUFFER, gl::GL_COLOR_ATTACHMENT0, gl::GL_TEXTURE_2D, moments->id(), 0);
    } else if (kind == ShadowTargetKind::Tex2DArray) {
        gl::framebufferTextureLayer(gl::GL_FRAMEBUFFER, gl::GL_DEPTH_ATTACHMENT, depth.id(), 0, layerOrFace);
        if (moments)
            gl::framebufferTextureLayer(gl::GL_FRAMEBUFFER, gl::GL_COLOR_ATTACHMENT0, moments->id(), 0, layerOrFace);
    } else { // Cube: one face per glFramebufferTexture2D
        const gl::GLenum face = gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<gl::GLenum>(layerOrFace);
        f.glFramebufferTexture2D(gl::GL_FRAMEBUFFER, gl::GL_DEPTH_ATTACHMENT, face, depth.id(), 0);
        if (moments)
            f.glFramebufferTexture2D(gl::GL_FRAMEBUFFER, gl::GL_COLOR_ATTACHMENT0, face, moments->id(), 0);
    }
}

// deterministic RNG for the sample-pattern generators (xorshift32)
struct Xorshift {
    unsigned s;
    explicit Xorshift(unsigned seed) : s(seed ? seed : 0x9E3779B9u) {}
    unsigned next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    float unit() { return (next() >> 8) * (1.f / 16777216.f); } // [0,1)
};

} // namespace

struct ShadowTarget::Impl {};

ShadowTarget::ShadowTarget() : impl_(std::make_unique<Impl>()) {}
ShadowTarget::~ShadowTarget() = default;
ShadowTarget::ShadowTarget(ShadowTarget&&) noexcept = default;
ShadowTarget& ShadowTarget::operator=(ShadowTarget&&) noexcept = default;

bool ShadowTarget::create(ShadowTargetKind kind, int size, int layers, bool moments) {
    if (size <= 0) return false;
    kind_ = kind;
    size_ = size;
    layers_ = kind == ShadowTargetKind::Cube ? 6 : math::clamp(layers, 1, 64);
    moments_ = moments;

    const Texture::Target texTarget = kind == ShadowTargetKind::Tex2D      ? Texture::Target::Tex2D
                                       : kind == ShadowTargetKind::Tex2DArray ? Texture::Target::Tex2DArray
                                                                              : Texture::Target::Cube;
    configureDepthTexture(depthTex_, texTarget, gl::GL_DEPTH_COMPONENT24, size_, layers_);
    if (moments_)
        configureDepthTexture(momentsTex_, texTarget, gl::GL_RGBA32F, size_, layers_);

    fbo_.create();
    fbo_.setSize(size_, size_);
    fbo_.bind();
    attachDepth(kind_, depthTex_, moments_ ? &momentsTex_ : nullptr, 0);
    gl::drawBuffer(moments_ ? gl::GL_COLOR_ATTACHMENT0 : gl::GL_NONE);
    gl::readBuffer(gl::GL_NONE);
    if (!fbo_.isComplete()) {
        std::fprintf(stderr, "ShadowTarget: FBO incomplete (0x%x)\n", fbo_.status());
        Framebuffer::unbind();
        return false;
    }
    Framebuffer::unbind();

    if (!buildDepthMaterial(depthMaterial_, false)) return false;
    if (moments_ && !buildDepthMaterial(momentsMaterial_, true)) return false;
    return true;
}

void ShadowTarget::render(const SceneView& scene, const math::Mat4& vp, int layerOrFace) {
    if (!fbo_.id() || size_ <= 0) return;
    const int layer = kind_ == ShadowTargetKind::Cube ? math::clamp(layerOrFace, 0, 5)
                                                       : math::clamp(layerOrFace, 0, layers_ - 1);

    fbo_.bind(Framebuffer::Target::Draw);
    attachDepth(kind_, depthTex_, moments_ ? &momentsTex_ : nullptr, layer);
    gl::viewport(0, 0, size_, size_);
    if (moments_) {
        gl::clearColor(1.f, 1.f, 1.f, 1.f); // moments cleared to "no occluder"
        gl::clear(gl::ClearBit::Depth | gl::ClearBit::Color);
    } else {
        gl::clear(gl::ClearBit::Depth);
    }

    gl::enable(gl::Cap::DepthTest);
    gl::depthFunc(gl::CompareFunc::Less);
    gl::depthMask(true);
    gl::enable(gl::Cap::CullFace);
    gl::cullFace(gl::Face::Front); // back faces: less shadow acne
    gl::frontFace(true);

    Material& mat = moments_ ? momentsMaterial_ : depthMaterial_;
    mat.bind();
    drawSceneDepth(scene, mat.program(), vp);

    gl::cullFace(gl::Face::Back);
    Framebuffer::unbind(Framebuffer::Target::Draw);
}

std::array<math::Mat4, 6> pointLightFaceVPs(const math::Vec3& pos, float zNear, float zFar, float aspect) {
    const float fov = 0.5f * math::kPi; // 90 degrees per cube face
    const math::Mat4 proj = math::perspective(fov, aspect, zNear, zFar);
    const math::Vec3 dirs[6] = {{1.f, 0.f, 0.f}, {-1.f, 0.f, 0.f}, {0.f, 1.f, 0.f},
                                {0.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, -1.f}};
    // up vectors per face (avoid a parallel up): +Y/-Y use +/-Z, the rest -Y
    const math::Vec3 ups[6] = {{0.f, -1.f, 0.f}, {0.f, -1.f, 0.f}, {0.f, 0.f, 1.f},
                               {0.f, 0.f, -1.f}, {0.f, -1.f, 0.f}, {0.f, -1.f, 0.f}};
    std::array<math::Mat4, 6> vps{};
    for (int i = 0; i < 6; ++i)
        vps[i] = proj * math::lookAt(pos, pos + dirs[i], ups[i]);
    return vps;
}

math::Mat4 snapLightVPToTexels(const math::Mat4& lightVP, int mapSize, const math::Vec3& referenceWorldPos) {
    if (mapSize <= 0) return lightVP;
    const math::Vec4 c = lightVP * math::Vec4(referenceWorldPos, 1.f);
    if (std::fabs(c.w) < 1e-8f) return lightVP;
    const float nx = c.x / c.w, ny = c.y / c.w;
    const float tx = (nx * 0.5f + 0.5f) * static_cast<float>(mapSize);
    const float ty = (ny * 0.5f + 0.5f) * static_cast<float>(mapSize);
    // shift the clip-space x/y so the reference lands on a texel center (ndc += d)
    const float dx = (std::round(tx) - tx) / static_cast<float>(mapSize) * 2.f;
    const float dy = (std::round(ty) - ty) / static_cast<float>(mapSize) * 2.f;
    if (std::fabs(dx) < 1e-7f && std::fabs(dy) < 1e-7f) return lightVP;
    return math::translate({dx, dy, 0.f}) * lightVP;
}

CubeShadowMap::CubeShadowMap() = default;
CubeShadowMap::~CubeShadowMap() = default;
CubeShadowMap::CubeShadowMap(CubeShadowMap&&) noexcept = default;
CubeShadowMap& CubeShadowMap::operator=(CubeShadowMap&&) noexcept = default;

bool CubeShadowMap::create(int size, bool moments) {
    return target_.create(ShadowTargetKind::Cube, size, 6, moments);
}

void CubeShadowMap::render(const SceneView& scene, const Light& pointLight, float zNear, float zFar) {
    zNear_ = zNear;
    zFar_ = zFar > zNear ? zFar : zNear + 1.f;
    faceVPs_ = pointLightFaceVPs(pointLight.position, zNear_, zFar_);
    for (int f = 0; f < 6; ++f)
        target_.render(scene, faceVPs_[f], f);
}

ShadowAtlas::ShadowAtlas() = default;
ShadowAtlas::~ShadowAtlas() = default;
ShadowAtlas::ShadowAtlas(ShadowAtlas&&) noexcept = default;
ShadowAtlas& ShadowAtlas::operator=(ShadowAtlas&&) noexcept = default;

bool ShadowAtlas::create(int size, int layers, bool moments) {
    return target_.create(ShadowTargetKind::Tex2DArray, size, layers, moments);
}

void ShadowAtlas::renderLight(const SceneView& scene, int layer, const Light& light,
                              const math::Vec3& center, float radius, bool snap, float zNear, float zFar) {
    if (layer < 0 || layer >= target_.layers()) return;
    math::Mat4 vp = lightViewProj(light, center, radius, zNear, zFar);
    if (snap) vp = snapLightVPToTexels(vp, target_.size(), center);
    vps_[layer] = vp;
    target_.render(scene, vp, layer);
}

void ShadowAtlas::renderLayer(const SceneView& scene, int layer, const math::Mat4& vp) {
    if (layer < 0 || layer >= target_.layers()) return;
    vps_[layer] = vp;
    target_.render(scene, vp, layer);
}

std::vector<math::Vec3> hemisphereKernel(int samples, unsigned seed) {
    std::vector<math::Vec3> k;
    if (samples <= 0) return k;
    k.reserve(static_cast<std::size_t>(samples));
    Xorshift rng(seed);
    for (int i = 0; i < samples; ++i) {
        const float z = rng.unit();                  // hemisphere z in [0,1)
        const float phi = 2.f * math::kPi * rng.unit();
        const float r = std::sqrt(std::max(0.f, 1.f - z * z));
        const float scale = 0.1f + 0.9f * (static_cast<float>(i) / std::max(1, samples - 1)); // bias to origin
        k.push_back({std::cos(phi) * r * scale, std::sin(phi) * r * scale, z});
    }
    return k;
}

std::vector<math::Vec2> poissonDisk(int samples, unsigned seed) {
    std::vector<math::Vec2> d;
    if (samples <= 0) return d;
    d.reserve(static_cast<std::size_t>(samples));
    Xorshift rng(seed);
    for (int i = 0; i < samples; ++i) {
        const float a = 2.39996323f * static_cast<float>(i) + rng.unit() * 0.5f; // golden angle + jitter
        const float r = std::sqrt((static_cast<float>(i) + 0.5f + rng.unit() * 0.5f) / static_cast<float>(samples));
        d.push_back({std::cos(a) * r, std::sin(a) * r});
    }
    return d;
}

} // namespace fuhgl::render
