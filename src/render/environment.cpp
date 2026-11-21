#include "fuhgl/render/environment.hpp"

#include "fuhgl/gl/loader.hpp"
#include "fuhgl/gl/state.hpp"

#include <cmath>
#include <cstdio>

namespace fuhgl::render {
namespace {

constexpr float kPi = math::kPi;

math::Vec3 tangentFrameT(const math::Vec3& n) {
    const math::Vec3 up = std::fabs(n.y) < 0.999f ? math::Vec3{0.f, 1.f, 0.f} : math::Vec3{1.f, 0.f, 0.f};
    return math::normalize(math::cross(up, n));
}

} // namespace

math::Vec3 cubemapTexelDir(int face, float u, float v) {
    const float s = 2.f * u - 1.f;
    const float t = 2.f * v - 1.f;
    math::Vec3 d;
    switch (face) {
    case 0: d = {1.f, -t, -s}; break;   // +X
    case 1: d = {-1.f, -t, s}; break;   // -X
    case 2: d = {s, 1.f, t}; break;     // +Y
    case 3: d = {s, -1.f, -t}; break;   // -Y
    case 4: d = {s, -t, 1.f}; break;    // +Z
    default: d = {-s, -t, -1.f}; break; // -Z
    }
    return math::normalize(d);
}

math::Vec3 sampleCubemap(const CubemapData& src, const math::Vec3& dir) {
    const float ax = std::fabs(dir.x), ay = std::fabs(dir.y), az = std::fabs(dir.z);
    int face;
    float s, t;
    if (ax >= ay && ax >= az) {
        if (dir.x >= 0.f) { face = 0; s = -dir.z / dir.x; t = -dir.y / dir.x; }
        else { face = 1; s = -dir.z / dir.x; t = dir.y / dir.x; }
    } else if (ay >= az) {
        if (dir.y >= 0.f) { face = 2; s = dir.x / dir.y; t = dir.z / dir.y; }
        else { face = 3; s = -dir.x / dir.y; t = dir.z / dir.y; }
    } else {
        if (dir.z >= 0.f) { face = 4; s = dir.x / dir.z; t = -dir.y / dir.z; }
        else { face = 5; s = dir.x / dir.z; t = dir.y / dir.z; }
    }
    const float u = (s + 1.f) * 0.5f;
    const float v = (t + 1.f) * 0.5f;
    const int size = src.size;
    int i = static_cast<int>(u * size);
    int j = static_cast<int>(v * size);
    if (i < 0) i = 0;
    if (i >= size) i = size - 1;
    if (j < 0) j = 0;
    if (j >= size) j = size - 1;
    const float* px = &src.face[face][(static_cast<std::size_t>(j) * size + i) * 4];
    return {px[0], px[1], px[2]};
}

math::Vec2 hammersley(std::uint32_t i, std::uint32_t n) {
    return {n > 0 ? static_cast<float>(i) / n : 0.f, radicalInverse(i)};
}

math::Vec3 importanceSampleGGX(const math::Vec2& xi, float roughness, const math::Vec3& n) {
    const float a = roughness * roughness;
    const float phi = 2.f * kPi * xi.x;
    const float cosTheta = std::sqrt((1.f - xi.y) / (1.f + (a * a - 1.f) * xi.y));
    const float sinTheta = std::sqrt(math::clamp(1.f - cosTheta * cosTheta, 0.f, 1.f));
    const math::Vec3 hLocal{sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta};
    const math::Vec3 t = tangentFrameT(n);
    const math::Vec3 b = math::cross(n, t);
    return math::normalize(t * hLocal.x + b * hLocal.y + n * hLocal.z);
}

float geometrySchlickGGX(float ndotx, float k) {
    return ndotx / (ndotx * (1.f - k) + k);
}

bool EnvironmentBake::valid() const {
    if (irradianceSize <= 0 || brdfLutSize <= 0 || prefilterSize <= 0 || prefilterLevels <= 0) return false;
    const std::size_t irrNeed = static_cast<std::size_t>(irradianceSize) * irradianceSize * 3;
    for (int f = 0; f < 6; ++f)
        if (irradianceFace[f].size() != irrNeed) return false;
    std::size_t preNeed = 0;
    for (int m = 0; m < prefilterLevels; ++m) {
        const int s = prefilterSize >> m;
        preNeed += static_cast<std::size_t>(s) * s * 3;
    }
    for (int f = 0; f < 6; ++f)
        if (prefilterFaceLevel[f].size() != preNeed) return false;
    return brdfLut.size() == static_cast<std::size_t>(brdfLutSize) * brdfLutSize * 2;
}

bool bakeEnvironment(const CubemapData& src, EnvironmentBake& out, int irradianceSize, int prefilterSize,
                     int brdfLutSize) {
    if (!src.valid() || irradianceSize <= 0 || prefilterSize <= 0 || brdfLutSize <= 0) return false;
    out = EnvironmentBake{};
    out.irradianceSize = irradianceSize;
    out.prefilterSize = prefilterSize;
    out.brdfLutSize = brdfLutSize;
    out.prefilterLevels = 1;
    while ((prefilterSize >> out.prefilterLevels) > 0) ++out.prefilterLevels;

    // --- irradiance: cosine-weighted hemisphere average around each texel normal ---
    constexpr std::uint32_t kIrrSamples = 512;
    for (int f = 0; f < 6; ++f) {
        out.irradianceFace[f].assign(static_cast<std::size_t>(irradianceSize) * irradianceSize * 3, 0.f);
        for (int j = 0; j < irradianceSize; ++j)
            for (int i = 0; i < irradianceSize; ++i) {
                const math::Vec3 n = cubemapTexelDir(f, (i + 0.5f) / irradianceSize, (j + 0.5f) / irradianceSize);
                const math::Vec3 t = tangentFrameT(n);
                const math::Vec3 b = math::cross(n, t);
                math::Vec3 sum{0.f, 0.f, 0.f};
                for (std::uint32_t k = 0; k < kIrrSamples; ++k) {
                    const math::Vec2 xi = hammersley(k, kIrrSamples);
                    const float phi = 2.f * kPi * xi.x;
                    const float r = std::sqrt(xi.y);
                    const math::Vec3 local{r * std::cos(phi), r * std::sin(phi), std::sqrt(1.f - xi.y)};
                    const math::Vec3 dir = t * local.x + b * local.y + n * local.z;
                    sum += sampleCubemap(src, dir);
                }
                const math::Vec3 irr = sum / static_cast<float>(kIrrSamples);
                float* px = &out.irradianceFace[f][(static_cast<std::size_t>(j) * irradianceSize + i) * 3];
                px[0] = irr.x; px[1] = irr.y; px[2] = irr.z;
            }
    }

    // --- prefilter: GGX importance sampling of the source env, roughness per mip ---
    constexpr std::uint32_t kPreSamples = 512;
    for (int f = 0; f < 6; ++f) {
        std::size_t offset = 0;
        out.prefilterFaceLevel[f].assign(
            [&] {
                std::size_t n = 0;
                for (int m = 0; m < out.prefilterLevels; ++m) {
                    const int s = prefilterSize >> m;
                    n += static_cast<std::size_t>(s) * s * 3;
                }
                return n;
            }(),
            0.f);
        for (int m = 0; m < out.prefilterLevels; ++m) {
            const int size = prefilterSize >> m;
            const float roughness = out.prefilterLevels > 1 ? static_cast<float>(m) / (out.prefilterLevels - 1) : 0.f;
            for (int j = 0; j < size; ++j)
                for (int i = 0; i < size; ++i) {
                    const math::Vec3 n = cubemapTexelDir(f, (i + 0.5f) / size, (j + 0.5f) / size);
                    math::Vec3 sum{0.f, 0.f, 0.f};
                    float wsum = 0.f;
                    for (std::uint32_t k = 0; k < kPreSamples; ++k) {
                        const math::Vec2 xi = hammersley(k, kPreSamples);
                        const math::Vec3 h = importanceSampleGGX(xi, roughness, n);
                        const math::Vec3 l = math::normalize(2.f * math::dot(n, h) * h - n); // reflect(-n, h)
                        const float ndotl = math::clamp(math::dot(n, l), 0.f, 1.f);
                        if (ndotl > 0.f) {
                            sum += sampleCubemap(src, l) * ndotl;
                            wsum += ndotl;
                        }
                    }
                    const math::Vec3 pre = wsum > 0.f ? sum / wsum : math::Vec3{0.f, 0.f, 0.f};
                    float* px = &out.prefilterFaceLevel[f][offset + (static_cast<std::size_t>(j) * size + i) * 3];
                    px[0] = pre.x; px[1] = pre.y; px[2] = pre.z;
                }
            offset += static_cast<std::size_t>(size) * size * 3;
        }
    }

    // --- BRDF LUT: integrate the GGX VNDF (Epic-style scale/bias for the specular IBL) ---
    constexpr std::uint32_t kLutSamples = 1024;
    out.brdfLut.assign(static_cast<std::size_t>(brdfLutSize) * brdfLutSize * 2, 0.f);
    for (int j = 0; j < brdfLutSize; ++j)
        for (int i = 0; i < brdfLutSize; ++i) {
            const float ndotv = (i + 0.5f) / brdfLutSize;
            const float roughness = (j + 0.5f) / brdfLutSize;
            const math::Vec3 v{std::sqrt(math::clamp(1.f - ndotv * ndotv, 0.f, 1.f)), 0.f, ndotv};
            const float k = roughness * roughness * 0.5f;
            float a = 0.f, b = 0.f;
            for (std::uint32_t s = 0; s < kLutSamples; ++s) {
                const math::Vec2 xi = hammersley(s, kLutSamples);
                const math::Vec3 h = importanceSampleGGX(xi, roughness, {0.f, 0.f, 1.f});
                const math::Vec3 vh = math::normalize(2.f * math::dot(v, h) * h - v);
                const float ndotl = math::clamp(vh.z, 0.f, 1.f);
                const float ndoth = math::clamp(h.z, 0.f, 1.f);
                const float vdoth = math::clamp(math::dot(vh, h), 0.f, 1.f);
                if (ndotl > 0.f) {
                    const float g = geometrySchlickGGX(ndotl, k) * geometrySchlickGGX(ndotv, k);
                    const float gvis = g * vdoth / std::max(ndoth * ndotv, 1e-5f);
                    const float fc = std::pow(1.f - vdoth, 5.f);
                    a += (1.f - fc) * gvis;
                    b += fc * gvis;
                }
            }
            float* px = &out.brdfLut[(static_cast<std::size_t>(j) * brdfLutSize + i) * 2];
            px[0] = a / kLutSamples;
            px[1] = b / kLutSamples;
        }
    return true;
}

CubemapData readCubemap(const Texture& cube) {
    CubemapData out;
    if (!cube.id()) return out;
    cube.bind();
    // query via a face target: some drivers return 0 for GL_TEXTURE_CUBE_MAP level queries
    gl::GLint size = 0, internal = 0;
    if (gl::functions().glGetTexLevelParameteriv) {
        gl::functions().glGetTexLevelParameteriv(gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, gl::GL_TEXTURE_WIDTH, &size);
        gl::functions().glGetTexLevelParameteriv(gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, gl::GL_TEXTURE_INTERNAL_FORMAT,
                                                  &internal);
    }
    out.size = size;
    if (out.size <= 0) return CubemapData{};
    const bool isFloat = (internal == gl::GL_RGB16F || internal == gl::GL_RGB32F || internal == gl::GL_RGBA16F ||
                          internal == gl::GL_RGBA32F || internal == gl::GL_RG16F || internal == gl::GL_RG32F);
    for (int f = 0; f < 6; ++f) {
        const std::size_t n = static_cast<std::size_t>(out.size) * out.size;
        out.face[f].assign(n * 4, 0.f);
        if (isFloat) {
            gl::getTexImage(gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, gl::GL_RGBA, gl::GL_FLOAT, out.face[f].data());
        } else {
            std::vector<unsigned char> bytes(n * 4);
            gl::getTexImage(gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, gl::GL_RGBA, gl::GL_UNSIGNED_BYTE, bytes.data());
            for (std::size_t p = 0; p < n * 4; ++p) out.face[f][p] = bytes[p] / 255.f;
        }
    }
    return out;
}

struct Environment::Impl {};

Environment::Environment() : impl_(std::make_unique<Impl>()) {}
Environment::~Environment() = default;
Environment::Environment(Environment&&) noexcept = default;
Environment& Environment::operator=(Environment&&) noexcept = default;

bool Environment::createFromCubemap(const Texture& srcCube, int irradianceSize, int prefilterSize,
                                    int brdfLutSize) {
    const CubemapData src = readCubemap(srcCube);
    EnvironmentBake bake;
    if (!bakeEnvironment(src, bake, irradianceSize, prefilterSize, brdfLutSize)) {
        std::fprintf(stderr, "Environment: bake failed (invalid source cubemap?)\n");
        return false;
    }
    prefilterLevels_ = bake.prefilterLevels;

    // irradiance cubemap (RGB16F, single level, linear)
    irradianceTex_ = Texture(Texture::Target::Cube);
    irradianceTex_.bind();
    for (int f = 0; f < 6; ++f)
        irradianceTex_.setImageCubeFace(static_cast<Texture::CubeFace>(gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X + f), 0,
                                        gl::GL_RGB16F, bake.irradianceSize, gl::GL_RGB, gl::GL_FLOAT,
                                        bake.irradianceFace[f].data());
    irradianceTex_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR);
    irradianceTex_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
    irradianceTex_.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    irradianceTex_.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    irradianceTex_.setParameteri(gl::GL_TEXTURE_WRAP_R, gl::GL_CLAMP_TO_EDGE);

    // prefiltered cubemap (RGB16F, full mip chain, linear-mipmap-linear)
    prefilterTex_ = Texture(Texture::Target::Cube);
    prefilterTex_.bind();
    for (int f = 0; f < 6; ++f) {
        std::size_t offset = 0;
        for (int m = 0; m < bake.prefilterLevels; ++m) {
            const int size = bake.prefilterSize >> m;
            prefilterTex_.setImageCubeFace(static_cast<Texture::CubeFace>(gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X + f), m,
                                           gl::GL_RGB16F, size, gl::GL_RGB, gl::GL_FLOAT,
                                           bake.prefilterFaceLevel[f].data() + offset);
            offset += static_cast<std::size_t>(size) * size * 3;
        }
    }
    prefilterTex_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR_MIPMAP_LINEAR);
    prefilterTex_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
    prefilterTex_.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    prefilterTex_.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    prefilterTex_.setParameteri(gl::GL_TEXTURE_WRAP_R, gl::GL_CLAMP_TO_EDGE);

    // BRDF LUT (RG16F 2D, linear)
    brdfLutTex_ = Texture(Texture::Target::Tex2D);
    brdfLutTex_.bind();
    brdfLutTex_.texStorage2D(1, gl::GL_RG16F, bake.brdfLutSize, bake.brdfLutSize);
    brdfLutTex_.setSubImage2D(0, 0, 0, bake.brdfLutSize, bake.brdfLutSize, gl::GL_RG, gl::GL_FLOAT,
                              bake.brdfLut.data());
    brdfLutTex_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR);
    brdfLutTex_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
    brdfLutTex_.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    brdfLutTex_.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    return true;
}

} // namespace fuhgl::render
