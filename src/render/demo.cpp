#include "fuhgl/render/demo.hpp"

#include "fuhgl/render/deferred_renderer.hpp"
#include "fuhgl/render/environment.hpp"
#include "fuhgl/render/forward_renderer.hpp"
#include "fuhgl/render/materials.hpp"
#include "fuhgl/render/mesh.hpp"
#include "fuhgl/render/procedural.hpp"
#include "fuhgl/render/solid.hpp"
#include "fuhgl/cam/first_person.hpp"
#include "fuhgl/skybox.hpp"

#include <cmath>
#include <vector>

namespace fuhgl::render {

namespace {

float hash01(int i, int j, int seed) {
    const float s = std::sin(i * 12.9898f + j * 78.233f + seed * 37.719f) * 43758.5453f;
    return s - std::floor(s);
}

} // namespace

struct DemoScene::Impl {
    DemoMode mode = DemoMode::Day;

    // terrain
    Mesh terrainMesh, waterMesh, monoMesh;
    Mesh pineTrunk, pineCanopy, broadTrunk, broadCanopy;
    Mesh bushMeshA, bushMeshB, rockMeshA, rockMeshB;
    Mesh grassMeshA, grassMeshB, lampMesh;
    Texture grassTex{Texture::Target::Tex2D}, rockTex{Texture::Target::Tex2D}, stoneTex{Texture::Target::Tex2D};
    Material terrainMat, trunkMat, pineMat, broadMat, bushMat, rockMat, stoneMat, waterMat, grassBladeMat, lampMat;

    Environment env;                 ///< IBL bake (night mode only)
    bool hasEnv = false;

    std::vector<DrawItem> items;
    std::vector<Light> lights;
    math::Vec3 clearColor{0.05f, 0.07f, 0.12f};
    math::Vec3 ambient{0.16f, 0.17f, 0.20f};
    math::Vec3 fogColor{0.65f, 0.75f, 0.90f};
    float fogDensity = 0.003f;
    math::Vec3 skyTop{0.16f, 0.40f, 0.85f};
    math::Vec3 skyBottom{0.72f, 0.80f, 0.92f};
    math::Vec3 monoPos{-4.5f, 0.f, -6.f};

    float heightAt(float x, float z) const {
        const float d = std::sqrt(x * x + z * z);
        float h = 2.6f * math::fbm2(x * 0.045f, z * 0.045f, 4, 7u)
                + 0.55f * math::fbm2(x * 0.17f, z * 0.17f, 3, 13u);
        const float flat = math::smoothstep(2.5f, 10.f, d);
        h = h * flat - 0.6f * (1.f - flat); // clearing dips slightly below zero
        const float pdx = x - 4.5f, pdz = z + 5.5f;   // pond basin at (4.5, -5.5)
        const float pd = std::sqrt(pdx * pdx + pdz * pdz);
        const float basin = math::smoothstep(8.f, 3.f, pd);
        h = h * (1.f - basin) + (-1.45f) * basin;
        h -= 1.1f * std::exp(-(pdx * pdx + pdz * pdz) / 30.f);
        return h;
    }

    math::Vec3 startPosition() const { return {0.f, heightAt(0.f, 17.f) + 3.4f, 17.f}; }

    bool buildTerrain() {
        const float kTerrainR = 30.f;
        const int kTerrainN = 88;
        MeshData terrainData;
        const float step = 2.f * kTerrainR / kTerrainN;
        terrainData.vertices.reserve(static_cast<size_t>(kTerrainN + 1) * (kTerrainN + 1) * MeshData::kFloatsPerVertex);
        for (int j = 0; j <= kTerrainN; ++j)
            for (int i = 0; i <= kTerrainN; ++i) {
                const float x = -kTerrainR + i * step, z = -kTerrainR + j * step;
                const float y = heightAt(x, z);
                const float e = step * 0.5f;
                const math::Vec3 n = math::normalize(
                    {heightAt(x - e, z) - heightAt(x + e, z), 2.f * e, heightAt(x, z - e) - heightAt(x, z + e)});
                const float g = 0.9f + 0.1f * math::fbm2(x * 0.3f, z * 0.3f, 2, 29u);
                const float v[] = {x, y, z, n.x, n.y, n.z, 0.92f, 0.95f * g, 0.88f, x * 0.22f, z * 0.22f};
                terrainData.vertices.insert(terrainData.vertices.end(), v, v + MeshData::kFloatsPerVertex);
            }
        terrainData.indices.reserve(static_cast<size_t>(kTerrainN) * kTerrainN * 6);
        for (int j = 0; j < kTerrainN; ++j)
            for (int i = 0; i < kTerrainN; ++i) {
                const unsigned v00 = static_cast<unsigned>(j * (kTerrainN + 1) + i);
                const unsigned v10 = v00 + 1, v01 = v00 + kTerrainN + 1, v11 = v01 + 1;
                const unsigned t[] = {v00, v01, v11, v00, v11, v10}; // CCW seen from +y
                terrainData.indices.insert(terrainData.indices.end(), t, t + 6);
            }
        terrainMesh.setMeshData(terrainData);
        grassTex = procedural::makeTexture(procedural::noise(256, 256, 7u, 5));
        const bool night = mode == DemoMode::Night, sunset = mode == DemoMode::Sunset;
        terrainMat = materials::pbrTextured(grassTex,
                                            night ? math::Vec3{0.10f, 0.13f, 0.16f}
                                            : sunset ? math::Vec3{0.38f, 0.30f, 0.16f}
                                                     : math::Vec3{0.30f, 0.50f, 0.22f},
                                            0.f, 0.95f);
        return true;
    }

    bool buildMeshes() {
        const bool night = mode == DemoMode::Night, sunset = mode == DemoMode::Sunset;

        // trees: CSG trunk + canopy, two species
        auto pineTrunkSolid = makeCylinder(0.09f, 1.3f);
        auto pineCanopySolid = translated(makeCone(0.9f, 2.2f), {0.f, 2.0f, 0.f})
                               | translated(makeCone(0.65f, 1.8f), {0.f, 3.3f, 0.f})
                               | translated(makeCone(0.4f, 1.4f), {0.f, 4.4f, 0.f});
        pineTrunk = pineTrunkSolid->buildMesh(14, {1.f, 1.f, 1.f});
        pineCanopy = pineCanopySolid->buildMesh(14, {1.f, 1.f, 1.f});
        auto broadTrunkSolid = makeCylinder(0.13f, 1.7f);
        auto broadCanopySolid = translated(makeSphere(1.0f), {0.f, 2.2f, 0.f})
                                | translated(makeSphere(0.7f), {0.55f, 2.9f, 0.2f})
                                | translated(makeSphere(0.6f), {-0.5f, 2.8f, -0.3f});
        broadTrunk = broadTrunkSolid->buildMesh(14, {1.f, 1.f, 1.f});
        broadCanopy = broadCanopySolid->buildMesh(14, {1.f, 1.f, 1.f});
        trunkMat = materials::matte(night ? math::Vec3{0.10f, 0.08f, 0.06f}
                                           : math::Vec3{0.30f, 0.21f, 0.13f});
        pineMat = materials::matte(night ? math::Vec3{0.05f, 0.10f, 0.07f}
                                  : sunset ? math::Vec3{0.16f, 0.22f, 0.10f}
                                           : math::Vec3{0.13f, 0.35f, 0.15f});
        broadMat = materials::matte(night ? math::Vec3{0.06f, 0.10f, 0.05f}
                                   : sunset ? math::Vec3{0.24f, 0.20f, 0.08f}
                                            : math::Vec3{0.22f, 0.42f, 0.16f});

        // bushes: CSG sphere blobs
        auto bushSolidA = makeSphere(0.55f) - translated(makeSphere(0.38f), {0.3f, 0.25f, 0.15f});
        auto bushSolidB = makeSphere(0.45f) - translated(makeSphere(0.3f), {-0.25f, 0.2f, -0.15f});
        bushMeshA = bushSolidA->buildMesh(12, {1.f, 1.f, 1.f});
        bushMeshB = bushSolidB->buildMesh(12, {1.f, 1.f, 1.f});
        bushMat = materials::matte(night ? math::Vec3{0.05f, 0.09f, 0.05f}
                                  : sunset ? math::Vec3{0.20f, 0.18f, 0.08f}
                                           : math::Vec3{0.17f, 0.36f, 0.13f});

        // rocks: two CSG blobs with a voronoi texture
        auto rockSolidA = makeSphere(0.9f) - translated(makeSphere(0.55f), {0.4f, 0.35f, 0.2f});
        auto rockSolidB = makeSphere(0.7f) - translated(makeSphere(0.5f), {-0.3f, 0.3f, -0.25f});
        rockMeshA = rockSolidA->buildMesh(16, {1.f, 1.f, 1.f});
        rockMeshB = rockSolidB->buildMesh(16, {1.f, 1.f, 1.f});
        rockTex = procedural::makeTexture(procedural::voronoi(128, 128, 8, 99u,
                                                              {0.16f, 0.15f, 0.14f}, {0.55f, 0.53f, 0.50f}));
        rockMat = materials::textured(rockTex, {1.f, 1.f, 1.f});

        // monolith: cube with a spherical bite, dark stone
        auto monoSolid = makeCube({0.7f, 2.6f, 0.7f}) - makeSphere(0.55f);
        monoMesh = monoSolid->buildMesh(28, {1.f, 1.f, 1.f});
        stoneTex = procedural::makeTexture(procedural::voronoi(128, 128, 5, 5u,
                                                               {0.10f, 0.10f, 0.12f}, {0.38f, 0.36f, 0.34f}));
        stoneMat = materials::textured(stoneTex, {1.f, 1.f, 1.f});

        // pond: metallic mirror-smooth water slab (SSR sky reflection)
        shapes::box(waterMesh, {4.6f, 0.06f, 3.6f}, {1.f, 1.f, 1.f});
        waterMat = materials::pbr({0.06f, 0.13f, 0.19f}, 1.0f, 0.04f);

        // grass: two thin blade meshes, thousands of F24 instances
        auto makeBlade = [](float w, float h) {
            MeshData d = shapes::boxData({w, h, w}, {1.f, 1.f, 1.f});
            for (size_t v = 0; v < d.vertexCount(); ++v) {
                float* p = d.vertices.data() + v * MeshData::kFloatsPerVertex;
                p[1] += h; // base at y = 0
                const float t = math::clamp(p[1] / (2.f * h), 0.f, 1.f); // dark base -> light tip
                p[6] = 0.10f + 0.25f * t;
                p[7] = 0.24f + 0.31f * t;
                p[8] = 0.08f + 0.12f * t;
            }
            Mesh m;
            m.setMeshData(d);
            return m;
        };
        grassMeshA = makeBlade(0.014f, 0.32f);
        grassMeshB = makeBlade(0.022f, 0.5f);
        grassBladeMat = materials::matte({1.f, 1.f, 1.f}); // vertex colors carry the gradient
        std::vector<math::Mat4> grassModelsA, grassModelsB;
        grassModelsA.reserve(2000);
        grassModelsB.reserve(1000);
        for (int i = 0; i < 3000; ++i) {
            const float x = (hash01(i, 8, 31) - 0.5f) * 46.f;
            const float z = (hash01(i, 9, 32) - 0.5f) * 46.f - 5.f;
            if (std::sqrt(x * x + (z + 2.f) * (z + 2.f)) < 2.2f) continue;          // keep the clearing walkable
            if (std::sqrt((x - 4.5f) * (x - 4.5f) + (z + 5.5f) * (z + 5.5f)) < 3.5f) continue; // keep out of the pond
            const float y = heightAt(x, z);
            const math::Mat4 m = math::translate({x, y, z}) *
                                 math::rotateY(hash01(i, 10, 33) * 6.2831853f) *
                                 math::rotateX((hash01(i, 11, 34) - 0.5f) * 0.35f) *
                                 math::scale({1.f, 0.7f + 0.6f * hash01(i, 12, 35), 1.f});
            if (i < 2000) grassModelsA.push_back(m);
            else grassModelsB.push_back(m);
        }
        grassMeshA.setInstanceData(grassModelsA.data(), static_cast<gl::GLsizei>(grassModelsA.size()));
        grassMeshB.setInstanceData(grassModelsB.data(), static_cast<gl::GLsizei>(grassModelsB.size()));
        return true;
    }

    void buildScene() {
        items.clear();
        lights.clear();
        items.push_back({&terrainMesh, &terrainMat, math::Mat4{}});
        items.push_back({&waterMesh, &waterMat, math::translate({4.5f, -0.95f, -5.5f})});
        monoPos = {-4.5f, heightAt(-4.5f, -6.f) + 2.4f, -6.f};
        items.push_back({&monoMesh, &stoneMat, math::translate(monoPos) * math::rotateY(0.5f)});

        // trees + bushes + rocks scattered over the hills (deterministic jitter, avoid clearing/pond)
        for (int i = 0; i < 40; ++i) {
            const float x = (hash01(i, 3, 21) - 0.5f) * 52.f;
            const float z = (hash01(i, 4, 22) - 0.5f) * 52.f - 6.f;
            if (std::abs(x) > 26.f || std::abs(z) > 26.f) continue;
            if (std::sqrt(x * x + (z + 2.f) * (z + 2.f)) < 3.5f) continue;
            if (std::sqrt((x - 4.5f) * (x - 4.5f) + (z + 5.5f) * (z + 5.5f)) < 4.5f) continue;
            const float y = heightAt(x, z);
            const float s = 0.7f + hash01(i, 5, 23) * 0.9f;
            const math::Mat4 t = math::translate({x, y - 0.05f, z}) *
                                 math::rotateY(hash01(i, 6, 24) * 6.2831853f) *
                                 math::scale({s, s, s});
            if (hash01(i, 7, 25) > 0.6f) {
                items.push_back({&broadTrunk, &trunkMat, t});
                items.push_back({&broadCanopy, &broadMat, t});
            } else {
                items.push_back({&pineTrunk, &trunkMat, t});
                items.push_back({&pineCanopy, &pineMat, t});
            }
        }
        for (int i = 0; i < 22; ++i) {
            const float x = (hash01(i, 13, 41) - 0.5f) * 44.f;
            const float z = (hash01(i, 14, 42) - 0.5f) * 44.f - 5.f;
            if (std::sqrt(x * x + (z + 2.f) * (z + 2.f)) < 2.6f) continue;
            if (std::sqrt((x - 4.5f) * (x - 4.5f) + (z + 5.5f) * (z + 5.5f)) < 3.8f) continue;
            const float s = 0.5f + hash01(i, 15, 43) * 0.9f;
            Mesh& bm = (i % 2 == 0) ? bushMeshA : bushMeshB;
            items.push_back({&bm, &bushMat,
                             math::translate({x, heightAt(x, z) - 0.08f, z}) *
                             math::rotateY(hash01(i, 16, 44) * 6.2831853f) *
                             math::scale({s, s * 0.75f, s})});
        }
        for (int i = 0; i < 14; ++i) {
            const float x = (hash01(i, 1, 15) - 0.5f) * 48.f;
            const float z = (hash01(i, 2, 16) - 0.5f) * 48.f - 5.f;
            if (std::sqrt(x * x + (z + 2.f) * (z + 2.f)) < 2.4f) continue;
            if (std::sqrt((x - 4.5f) * (x - 4.5f) + (z + 5.5f) * (z + 5.5f)) < 3.6f) continue;
            const float s = 0.4f + hash01(i, 17, 45) * 1.4f;
            Mesh& rm = (i % 2 == 0) ? rockMeshA : rockMeshB;
            items.push_back({&rm, &rockMat,
                             math::translate({x, heightAt(x, z) + 0.05f * s, z}) *
                             math::rotateY(hash01(i, 18, 46) * 6.2831853f) *
                             math::scale({s, s * 0.8f, s})});
        }
        items.push_back({&grassMeshA, &grassBladeMat, math::Mat4{}});
        items.push_back({&grassMeshB, &grassBladeMat, math::Mat4{}});

        // sun: day = high warm light; sunset = low in front of the camera (god rays in
        // frame); night = dim blue "moon"
        const bool night = mode == DemoMode::Night, sunset = mode == DemoMode::Sunset;
        lights.push_back(night ? Light::directional({-0.3f, -1.f, -0.2f}, {0.35f, 0.45f, 0.70f}, 0.9f)
                     : sunset ? Light::directional({0.25f, -0.10f, 1.f}, {1.f, 0.45f, 0.20f}, 3.0f)
                              : Light::directional({-0.4f, -1.f, -0.25f}, {1.f, 0.96f, 0.88f}, 2.6f));
        lights.push_back(Light::point({-6.f, 5.f, 4.f},
                                      night ? math::Vec3{0.9f, 0.55f, 0.25f}
                                      : sunset ? math::Vec3{0.5f, 0.25f, 0.15f}
                                               : math::Vec3{0.45f, 0.55f, 0.85f}, 10.f, 18.f));

        clearColor = night ? math::Vec3{0.01f, 0.015f, 0.04f}
                     : sunset ? math::Vec3{0.10f, 0.05f, 0.10f}
                              : math::Vec3{0.05f, 0.07f, 0.12f};
        ambient = night ? math::Vec3{0.04f, 0.05f, 0.09f}
                  : sunset ? math::Vec3{0.20f, 0.12f, 0.10f}
                           : math::Vec3{0.16f, 0.17f, 0.20f};
        fogColor = night ? math::Vec3{0.02f, 0.03f, 0.07f}
                   : sunset ? math::Vec3{0.55f, 0.25f, 0.20f}
                            : math::Vec3{0.65f, 0.75f, 0.90f};
        fogDensity = night ? 0.004f : sunset ? 0.005f : 0.003f;
        skyTop = night ? math::Vec3{0.015f, 0.02f, 0.06f}
                 : sunset ? math::Vec3{0.10f, 0.07f, 0.22f}
                          : math::Vec3{0.16f, 0.40f, 0.85f};
        skyBottom = night ? math::Vec3{0.06f, 0.08f, 0.16f}
                    : sunset ? math::Vec3{0.95f, 0.42f, 0.16f}
                             : math::Vec3{0.72f, 0.80f, 0.92f};

        // night: IBL environment baked from a dark gradient cubemap + emissive lamp
        // posts whose reflections show up in the pond via SSR
        hasEnv = false;
        if (night) {
            Skybox nightSky;
            nightSky.create();
            nightSky.makeGradient({0.01f, 0.02f, 0.07f}, {0.10f, 0.12f, 0.22f}, 64);
            env.createFromCubemap(nightSky.texture());
            env.intensity = 1.0f;
            hasEnv = true;
            shapes::box(lampMesh, {0.12f, 0.9f, 0.12f}, {1.f, 1.f, 1.f});
            lampMat = materials::emissive({1.f, 0.72f, 0.35f}, 6.f);
            const math::Vec3 lampPos[3] = {{-3.f, 0.f, 2.f}, {3.5f, 0.f, -3.f}, {0.5f, 0.f, -7.f}};
            for (const auto& p : lampPos) {
                const float ly = heightAt(p.x, p.z);
                items.push_back({&lampMesh, &lampMat, math::translate({p.x, ly + 0.9f, p.z})});
                lights.push_back(Light::point({p.x, ly + 1.8f, p.z}, {1.f, 0.7f, 0.4f}, 14.f, 16.f));
            }
        }
    }
};

DemoScene::DemoScene() : impl_(std::make_unique<Impl>()) {}
DemoScene::~DemoScene() = default;
DemoScene::DemoScene(DemoScene&&) noexcept = default;
DemoScene& DemoScene::operator=(DemoScene&&) noexcept = default;

bool DemoScene::create(DemoMode mode) {
    impl_->mode = mode;
    if (!impl_->buildTerrain()) return false;
    if (!impl_->buildMeshes()) return false;
    impl_->buildScene();
    return true;
}

DemoMode DemoScene::mode() const { return impl_->mode; }

float DemoScene::heightAt(float x, float z) const { return impl_->heightAt(x, z); }

void DemoScene::applyStartPose(fuhgl::FirstPersonCamera& cam) const {
    cam.position = impl_->startPosition();
    cam.yaw = -0.08f;  // aim between monolith and pond
    cam.pitch = -0.17f;
}

float DemoScene::dofFocusDistance() const {
    return math::length(impl_->monoPos - impl_->startPosition());
}

void DemoScene::fillScene(SceneView& scene) const {
    scene.clearColor = impl_->clearColor;
    scene.ambient = impl_->ambient;
    scene.fogColor = impl_->fogColor;
    scene.fogDensity = impl_->fogDensity;
    scene.items = impl_->items;
    scene.lights = impl_->lights;
    scene.env = impl_->hasEnv ? &impl_->env : nullptr;
}

void DemoScene::configure(ForwardRenderer& r) const {
    const Impl& d = *impl_;
    const bool night = d.mode == DemoMode::Night, sunset = d.mode == DemoMode::Sunset;
    r.enableCsmShadows(2048, 4); // F30: cascaded shadow maps
    r.setShadowZRange(0.5f, 90.f);
    r.setShadowSceneBounds({0.f, 2.f, -4.f}, 24.f);
    r.setShadowStrength(1.f);
    r.setLights(d.lights);
    r.setAmbient(d.ambient);
    r.setClearColor(d.skyBottom); // forward has no sky pass: clear color is the sky
    r.setFog(d.fogColor, d.fogDensity);
    if (d.hasEnv) r.setEnvironment(&d.env);
    // F42: HDR forward pass + post chain (bloom, god rays, TAA, DoF, vol fog -> ACES LDR)
    r.enablePostFX(true);
    PostChain& pc = r.postChain();
    pc.bloomStrength = night ? 0.8f : 0.55f;
    pc.bloomThreshold = night ? 0.8f : 1.0f;
    pc.godRaysEnabled = true;
    pc.rayWeight = sunset ? 0.35f : 0.22f;
    pc.exposure = 1.0f;
    pc.taaEnabled = true;  // F32
    pc.dofEnabled = true;  // F36: focus on the monolith
    pc.dofFocusDist = dofFocusDistance();
    pc.dofFocusRange = 9.f;
    pc.dofStrength = 6.f;
    pc.dofTaps = 16;
    pc.volFogEnabled = true; // F37: morning mist (day), warm haze (sunset), thin night fog
    pc.volFogDensity = night ? 0.003f : sunset ? 0.005f : 0.004f;
    pc.volFogColor = night ? math::Vec3{0.05f, 0.07f, 0.12f}
                     : sunset ? math::Vec3{0.85f, 0.55f, 0.40f}
                              : math::Vec3{0.72f, 0.78f, 0.88f};
    pc.volFogAnisotropy = sunset ? 0.6f : 0.45f;
    pc.volFogHeight = 0.6f;
    pc.volFogFalloff = 0.16f;
    pc.volFogShadows = !night; // light shafts through the trees
    pc.volFogSteps = 24;
    pc.volFogMaxDist = 90.f;
}

void DemoScene::configure(DeferredRenderer& r) const {
    const Impl& d = *impl_;
    const bool night = d.mode == DemoMode::Night, sunset = d.mode == DemoMode::Sunset;
    r.enableCsmShadows(2048, 4); // F30: cascaded shadow maps
    r.setShadowZRange(0.5f, 90.f);
    r.setShadowSceneBounds({0.f, 2.f, -4.f}, 24.f);
    r.setShadowStrength(1.f);
    r.ssaoEnabled = true;
    r.ssaoRadius = 0.8f;
    r.ssaoStrength = 1.1f;
    r.bloomStrength = night ? 0.8f : 0.55f;
    r.bloomThreshold = night ? 0.8f : 1.0f;
    r.godRaysEnabled = true;
    r.rayWeight = sunset ? 0.35f : 0.22f;
    r.exposure = 1.0f;
    r.taaEnabled = true;  // F32
    r.ssrEnabled = true;  // F33
    r.ssrStrength = 0.55f;
    r.ssrMaxSteps = 48;
    r.ssrThickness = 0.5f;
    r.dofEnabled = true;  // F36: focus on the monolith
    r.dofFocusDist = dofFocusDistance();
    r.dofFocusRange = 9.f;
    r.dofStrength = 6.f;
    r.dofTaps = 16;
    r.volFogEnabled = true; // F37: morning mist (day), warm haze (sunset), thin night fog
    r.volFogDensity = night ? 0.003f : sunset ? 0.005f : 0.004f;
    r.volFogColor = night ? math::Vec3{0.05f, 0.07f, 0.12f}
                    : sunset ? math::Vec3{0.85f, 0.55f, 0.40f}
                             : math::Vec3{0.72f, 0.78f, 0.88f};
    r.volFogAnisotropy = sunset ? 0.6f : 0.45f;
    r.volFogHeight = 0.6f;
    r.volFogFalloff = 0.16f;
    r.volFogShadows = !night; // light shafts through the trees
    r.volFogSteps = 24;
    r.volFogMaxDist = 90.f;
    r.pipeline().skyTop = d.skyTop;
    r.pipeline().skyBottom = d.skyBottom;
}

const Environment* DemoScene::environment() const {
    return impl_->hasEnv ? &impl_->env : nullptr;
}

const char* DemoScene::title() const {
    switch (impl_->mode) {
        case DemoMode::Day: return "day";
        case DemoMode::Sunset: return "sunset";
        case DemoMode::Night: return "night";
    }
    return "day";
}

} // namespace fuhgl::render
