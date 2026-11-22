// Haunted mine (FuhGLCore render-layer showcase): a knight with sword, shield and lantern
// stands in a vast dark mine cavern while zombies lurk in ambush behind rocks and support
// beams in the fog. Everything is generated in code - no asset files.
//
// Features used (see docs/en/tutorials):
//   - DeferredRenderer with the full post stack: CSM shadows (F30), SSAO, HDR lighting,
//     bloom, crepuscular god rays from the ceiling light shaft, TAA (F32), SSR on the
//     puddle (F33), depth of field focused on the knight (F36) and shadowed volumetric
//     fog / light shafts (F37); distance fog + near-black sky.
//   - PBR metallic-roughness materials (F28) for armor/blade/shield/puddle, emissive
//     materials for flames, zombie eyes, crystals and the skylight.
//   - Procedural textures (F17): voronoi rock, noise floor, stripes wood, plus a baked
//     tangent-space normal map; parallax-occlusion floor and normal-mapped cavern walls.
//   - CSG solids (F18) for the cavern shell (cube & ellipsoid - light shaft), rocks,
//     stalactites, barrels, supports, the knight and the zombies; custom Solid subclass.
//   - Instancing (F24): rocks, stalactites/stalagmites, sleepers, torch poles/flames,
//     crystals, barrels and the whole zombie horde (one mesh, animated per frame via
//     updateInstanceData - the lurking sway).
//   - Heightmap terrain (F38) with LOD levels (F41) as the mine floor; objects placed
//     via heightmapSample().
//   - IBL (F29): Environment baked from a procedural dark Skybox cubemap.
//   - TextRenderer + FpsCounter HUD, Wireframe overlay (V key) showing the ambush
//     bounding boxes, effect toggles (F39/F40 API) on keys B/G/O/T/D/F/S/C/I/L.
//
// With --screenshot N headless frames are rendered (surfaceless EGL, deterministic time,
// TAA history accumulates) and written as PNG - used for CI-style verification.
//
// Usage: mine [--screenshot out.png] [--mode ambush|knight|horde] [--width W] [--height H]
//            [--frames N]
// Controls (windowed): WASD+mouse, Esc quits, V wireframe, B/G/O/T/D/F/S/C/I/L toggle
// effects (bloom/god rays/SSAO/TAA/DoF/vol fog/SSR/CSM/IBL/LOD).

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>
#include <fuhgl/skybox.hpp>

#include "../screenshot/png_writer.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using fuhgl::math::Mat4;
using fuhgl::math::Vec2;
using fuhgl::math::Vec3;
using fuhgl::math::Vec4;
using fuhgl::render::Effect;

struct Args {
    std::string screenshot;
    std::string mode = "ambush";
    int width = 960, height = 540;
    int frames = 10;
    bool wire = false;
    bool nodof = false;
    float focus = -1.f; // <0 = auto (per-mode default)
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) a.screenshot = argv[++i];
        else if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) a.mode = argv[++i];
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) a.width = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) a.height = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) a.frames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--wire") == 0) a.wire = true;
        else if (std::strcmp(argv[i], "--nodof") == 0) a.nodof = true;
        else if (std::strcmp(argv[i], "--focus") == 0 && i + 1 < argc) a.focus = std::atof(argv[++i]);
    }
    return a;
}

float hash01(int i, int j, int seed) {
    const float s = std::sin(i * 12.9898f + j * 78.233f + seed * 37.719f) * 43758.5453f;
    return s - std::floor(s);
}

// --- custom analytic solid: axis-aligned ellipsoid (used for the cavern shell) -----------

class Ellipsoid : public fuhgl::render::Solid {
public:
    explicit Ellipsoid(const Vec3& radii) : r_(radii) {}
    bool inside(const Vec3& p) const override {
        const float q = p.x * p.x / (r_.x * r_.x) + p.y * p.y / (r_.y * r_.y) + p.z * p.z / (r_.z * r_.z);
        return q <= 1.f;
    }
    Vec3 aabbMin() const override { return {-r_.x, -r_.y, -r_.z}; }
    Vec3 aabbMax() const override { return r_; }

private:
    Vec3 r_;
};

// --- MeshData helpers: transform / merge / flip (CPU-side mesh composition) -------------

fuhgl::render::MeshData xformMeshData(const fuhgl::render::MeshData& in, const Mat4& m) {
    fuhgl::render::MeshData out;
    out.vertices.reserve(in.vertices.size());
    out.indices = in.indices;
    const fuhgl::math::Mat3 r3(m);
    for (size_t v = 0; v < in.vertexCount(); ++v) {
        const float* p = in.vertices.data() + v * fuhgl::render::MeshData::kFloatsPerVertex;
        const Vec4 tp = m * Vec4{p[0], p[1], p[2], 1.f};
        const Vec3 n = fuhgl::math::normalize(r3 * Vec3{p[3], p[4], p[5]});
        const float o[] = {tp.x, tp.y, tp.z, n.x, n.y, n.z, p[6], p[7], p[8], p[9], p[10]};
        out.vertices.insert(out.vertices.end(), o, o + fuhgl::render::MeshData::kFloatsPerVertex);
    }
    return out;
}

void appendMeshData(fuhgl::render::MeshData& dst, const fuhgl::render::MeshData& src) {
    const unsigned base = static_cast<unsigned>(dst.vertexCount());
    dst.vertices.insert(dst.vertices.end(), src.vertices.begin(), src.vertices.end());
    if (src.indices.empty()) {
        for (unsigned i = 0; i < static_cast<unsigned>(src.vertexCount()); ++i)
            dst.indices.push_back(base + i);
    } else {
        for (unsigned i : src.indices) dst.indices.push_back(base + i);
    }
}

// Reverses triangle winding and negates normals: turns a closed CSG shell into an
// inward-facing surface (the pipelines cull back faces, so a cavern must be flipped).
void flipMeshData(fuhgl::render::MeshData& d) {
    for (size_t t = 0; t + 2 < d.indices.size(); t += 3)
        std::swap(d.indices[t + 1], d.indices[t + 2]);
    for (size_t v = 0; v < d.vertexCount(); ++v) {
        float* p = d.vertices.data() + v * fuhgl::render::MeshData::kFloatsPerVertex;
        p[3] = -p[3];
        p[4] = -p[4];
        p[5] = -p[5];
    }
}

// One CSG part placed by a local matrix; merged with the other parts into a single mesh.
struct Part {
    fuhgl::render::SolidPtr solid;
    Mat4 local{};
    int res = 14;
    fuhgl::render::SolidUv uv = fuhgl::render::SolidUv::PlanarXZ;
};

fuhgl::render::MeshData buildParts(const std::vector<Part>& parts, const Vec3& color) {
    fuhgl::render::MeshData out;
    for (const Part& p : parts)
        appendMeshData(out, xformMeshData(p.solid->buildMeshData(p.res, color, p.uv), p.local));
    return out;
}

constexpr float kFloorSizeX = 60.f;  // mine floor extents (terrain)
constexpr float kFloorSizeZ = 52.f;
constexpr float kFloorHScale = 2.6f; // heightmap [0,1] -> world height
constexpr int kFloorGrid = 129;

// --- the haunted mine scene (PIMPL: all GL resources live in Impl) -----------------------

class HauntedMine {
public:
    HauntedMine() : impl_(std::make_unique<Impl>()) {}
    ~HauntedMine() = default;
    HauntedMine(const HauntedMine&) = delete;
    HauntedMine& operator=(const HauntedMine&) = delete;

    bool create(int width, int height, const std::string& mode);
    void renderFrame(float time);                    // animate + renderer.render(scene)
    void drawHud(float fps);                         // text + optional wireframe into output()
    void resize(int w, int h);                       // renderer + scene viewport follow the window
    void setDofFocus(float d);
    void toggleWireframe() { wireframe_ = !wireframe_; }
    void toggleEffect(fuhgl::render::Effect e);      // uniform effect API (F39/F40)
    bool effectOn(fuhgl::render::Effect e) const;
    fuhgl::Framebuffer& output();
    fuhgl::FirstPersonCamera& camera() { return cam_; }
    int width() const { return width_; }
    int height() const { return height_; }
    const std::string& mode() const { return mode_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    fuhgl::FirstPersonCamera cam_;
    int width_ = 0, height_ = 0;
    std::string mode_ = "ambush";
    bool wireframe_ = false;
};

struct HauntedMine::Impl {
    fuhgl::render::DeferredRenderer renderer;
    fuhgl::render::SceneView scene;
    fuhgl::render::Environment env;
    fuhgl::Skybox sky;
    fuhgl::render::TextRenderer text;
    fuhgl::render::Wireframe wire;

    fuhgl::render::LodMesh floorLod; // must be stable: items store &floorLod

    // meshes (PIMPL-owned, items store pointers)
    fuhgl::render::Mesh cavern, floorPlaceholder;
    fuhgl::render::Mesh rockA, rockB, stalactite, stalagmite, support, sleeper, rail;
    fuhgl::render::Mesh cart, barrel, crystal, puddle, skylight, torchPole, flame;
    fuhgl::render::Mesh knightArmor, knightBlade, knightShield, knightPlume, lantern;
    fuhgl::render::Mesh zombieBody, zombieEyes;

    // materials
    fuhgl::render::Material cavernMat, floorMat, rockMat, woodMat, ironMat, crystalMat;
    fuhgl::render::Material puddleMat, skylightMat, flameMat, armorMat, bladeMat, shieldMat;
    fuhgl::render::Material plumeMat, lanternMat, zombieMat, eyeMat;

    // textures
    fuhgl::Texture cavernTex, cavernNormal, floorTex, floorHeight, woodTex, rockTex;

    // instanced placement (animated copies kept for updateInstanceData)
    std::vector<float> grid;                       // mine floor heightmap (placement + HUD)
    std::vector<Mat4> zombieRotScale, zombieModels, flameBase, flameModels;
    std::vector<Vec3> zombiePos;
    Vec3 knightPos{0.f, 0.f, -3.f};
    std::vector<float> torchBaseIntensity;
    std::vector<int> torchLightIndices;
    float knightDist = 10.f;
    bool iblOn = true;
};

float floorHeightAt(const std::vector<float>& grid, float x, float z) {
    const float u = (x + kFloorSizeX * 0.5f) / kFloorSizeX;
    const float v = (z + kFloorSizeZ * 0.5f) / kFloorSizeZ;
    return fuhgl::render::heightmapSample(grid.data(), kFloorGrid, kFloorGrid, u, v) * kFloorHScale;
}

bool HauntedMine::create(int width, int height, const std::string& mode) {
    using namespace fuhgl::render;
    width_ = width;
    height_ = height;
    mode_ = mode;
    Impl& s = *impl_;

    // --- renderer with the full effect stack -------------------------------------------
    if (!s.renderer.create(width, height)) {
        std::fprintf(stderr, "mine: renderer create failed\n");
        return false;
    }
    s.renderer.enableCsmShadows(1024, 4); // F30
    s.renderer.setShadowZRange(0.5f, 120.f);
    s.renderer.setShadowSceneBounds({0.f, 1.f, -6.f}, 48.f);
    s.renderer.setShadowStrength(1.f);
    s.renderer.ssaoEnabled = true;
    s.renderer.ssaoRadius = 0.8f;
    s.renderer.ssaoStrength = 1.1f;
    s.renderer.ssaoSamples = 16;
    s.renderer.bloomStrength = 0.85f;
    s.renderer.bloomThreshold = 0.9f;
    s.renderer.bloomBlurPasses = 2;
    s.renderer.godRaysEnabled = true;
    s.renderer.rayDensity = 0.9f;
    s.renderer.rayDecay = 0.95f;
    s.renderer.rayWeight = 0.3f;
    s.renderer.exposure = 1.25f;
    s.renderer.taaEnabled = true;  // F32
    s.renderer.taaBlend = 0.12f;
    s.renderer.ssrEnabled = true;  // F33 (puddle reflections)
    s.renderer.ssrStrength = 0.45f;
    s.renderer.ssrMaxSteps = 24;
    s.renderer.ssrThickness = 0.4f;
    s.renderer.volFogEnabled = true; // F37: mist + shadowed shaft through the ceiling hole
    s.renderer.volFogDensity = 0.014f;
    s.renderer.volFogColor = {0.045f, 0.06f, 0.075f};
    s.renderer.volFogAnisotropy = 0.55f;
    s.renderer.volFogHeight = 0.8f;
    s.renderer.volFogFalloff = 0.35f;
    s.renderer.volFogShadows = true;
    s.renderer.volFogSteps = 16;
    s.renderer.volFogMaxDist = 80.f;
    s.renderer.pipeline().skyTop = {0.10f, 0.14f, 0.24f};    // dim moonlit sky seen through the shaft
    s.renderer.pipeline().skyBottom = {0.22f, 0.28f, 0.42f};

    // --- mine floor: heightmap terrain with LOD levels (F38 + F41) ----------------------
    std::vector<float>& grid = s.grid;
    grid.assign(size_t(kFloorGrid) * kFloorGrid, 0.f);
    for (int j = 0; j < kFloorGrid; ++j)
        for (int i = 0; i < kFloorGrid; ++i) {
            const float x = -kFloorSizeX * 0.5f + (kFloorSizeX * i) / (kFloorGrid - 1);
            const float z = -kFloorSizeZ * 0.5f + (kFloorSizeZ * j) / (kFloorGrid - 1);
            float n = 0.72f * fuhgl::math::fbm2(x * 0.055f, z * 0.055f, 4, 77u)
                    + 0.28f * fuhgl::math::fbm2(x * 0.21f, z * 0.21f, 2, 78u);
            const float e = std::max(std::abs(x) / (kFloorSizeX * 0.5f), std::abs(z) / (kFloorSizeZ * 0.5f));
            const float bowl = fuhgl::math::smoothstep(0.78f, 1.f, e); // rise into the walls
            float h = n * 0.5f + bowl * 0.55f;
            h *= 1.f - 0.65f * std::exp(-(x * x + (z + 3.f) * (z + 3.f)) / 25.f); // flat knight clearing
            grid[size_t(j) * kFloorGrid + i] = fuhgl::math::clamp(h, 0.f, 1.f);
        }
    const auto floorH = [&](float x, float z) { return floorHeightAt(grid, x, z); };
    s.floorLod = terrainLods(grid.data(), kFloorGrid, kFloorGrid,
                             kFloorSizeX, kFloorSizeZ, kFloorHScale, 3, 60.f);

    // --- procedural textures + materials -------------------------------------------------
    s.cavernTex = procedural::makeTexture(procedural::voronoi(256, 256, 26, 5u,
                                                   {0.05f, 0.05f, 0.06f}, {0.24f, 0.23f, 0.22f}));
    const auto cavernHeight = procedural::heightmap(256, 256, 9u, 5, 0.07f);
    s.cavernNormal = procedural::makeNormalMap(cavernHeight, 2.5f);
    s.cavernMat = materials::normalMapped(s.cavernTex, s.cavernNormal, {0.85f, 0.82f, 0.78f});

    s.floorTex = procedural::makeTexture(procedural::noise(256, 256, 21u, 5,
                                               {0.05f, 0.045f, 0.04f}, {0.22f, 0.20f, 0.17f}));
    s.floorHeight = procedural::makeTexture(procedural::heightmap(256, 256, 33u, 6, 0.06f));
    s.floorMat = materials::parallax(s.floorTex, s.floorHeight, 0.05f, 10, {0.9f, 0.88f, 0.85f});

    s.rockTex = procedural::makeTexture(procedural::voronoi(128, 128, 10, 99u,
                                                {0.06f, 0.06f, 0.07f}, {0.30f, 0.29f, 0.27f}));
    s.rockMat = materials::pbrTextured(s.rockTex, {0.75f, 0.73f, 0.70f}, 0.f, 0.95f);
    s.woodTex = procedural::makeTexture(procedural::stripes(256, 256, 22, {0.10f, 0.06f, 0.03f},
                                                {0.24f, 0.15f, 0.08f}, false));
    s.woodMat = materials::pbrTextured(s.woodTex, {0.8f, 0.7f, 0.6f}, 0.f, 0.9f);
    s.ironMat = materials::pbr({0.16f, 0.16f, 0.18f}, 0.9f, 0.45f);
    s.crystalMat = materials::emissive({0.25f, 0.95f, 0.65f}, 4.5f);
    s.puddleMat = materials::pbr({0.02f, 0.045f, 0.055f}, 0.f, 0.05f);
    s.skylightMat = materials::emissive({0.6f, 0.75f, 1.0f}, 3.f);
    s.flameMat = materials::emissive({1.f, 0.5f, 0.12f}, 7.f);
    s.armorMat = materials::pbr({0.34f, 0.36f, 0.42f}, 0.85f, 0.42f);
    s.bladeMat = materials::pbr({0.75f, 0.78f, 0.85f}, 1.f, 0.18f);
    s.shieldMat = materials::pbr({0.38f, 0.07f, 0.05f}, 0.7f, 0.5f);
    s.plumeMat = materials::matte({0.5f, 0.05f, 0.05f});
    s.lanternMat = materials::emissive({1.f, 0.72f, 0.35f}, 5.f);
    s.zombieMat = materials::pbr({0.22f, 0.30f, 0.17f}, 0.f, 0.95f);
    s.zombieMat.emissive = {0.015f, 0.06f, 0.015f}; // faint sickly self-glow so silhouettes read in the dark
    s.eyeMat = materials::emissive({0.3f, 1.f, 0.2f}, 10.f);

    // --- cavern shell: cube & ellipsoid - light shaft, CPU-displaced + flipped -----------
    const Vec3 cavCenter{0.f, -2.f, 0.f};
    auto cavernSolid = (makeCube({36.f, 13.f, 27.f}) & std::make_shared<Ellipsoid>(Vec3{32.f, 14.f, 28.f}))
                       - translated(makeCylinder(3.f, 34.f), {6.f, 0.f, -10.f});
    cavernSolid = translated(cavernSolid, cavCenter);
    {
        MeshData d = cavernSolid->buildMeshData(40, {1.f, 1.f, 1.f}, SolidUv::Spherical);
        for (size_t v = 0; v < d.vertexCount(); ++v) {
            float* p = d.vertices.data() + v * MeshData::kFloatsPerVertex;
            const Vec3 pos{p[0], p[1], p[2]};
            const Vec3 radial = fuhgl::math::normalize(pos - cavCenter);
            const float dx = pos.x - 6.f, dz = pos.z + 10.f;
            const float nearShaft = fuhgl::math::smoothstep(4.5f, 3.f, std::sqrt(dx * dx + dz * dz));
            const float amp = (fuhgl::math::fbm3(pos.x * 0.07f + 13.7f, pos.y * 0.07f, pos.z * 0.07f, 4, 4242u) - 0.5f)
                              * 1.6f * nearShaft;
            const Vec3 q = pos + radial * amp;
            const Vec3 gn{fuhgl::math::fbm3(pos.x * 0.23f, pos.y * 0.23f + 7.f, pos.z * 0.23f, 2, 7u) - 0.5f,
                          fuhgl::math::fbm3(pos.x * 0.23f + 3.f, pos.y * 0.23f, pos.z * 0.23f + 5.f, 2, 8u) - 0.5f,
                          fuhgl::math::fbm3(pos.x * 0.23f + 9.f, pos.y * 0.23f + 2.f, pos.z * 0.23f, 2, 9u) - 0.5f};
            const Vec3 n = fuhgl::math::normalize(radial + gn * 0.7f);
            p[0] = q.x; p[1] = q.y; p[2] = q.z;
            p[3] = n.x; p[4] = n.y; p[5] = n.z;
        }
        flipMeshData(d); // render the shell from inside (pipelines cull back faces)
        s.cavern.setMeshData(d);
    }

    // --- rocks (two CSG variants), stalactites/stalagmites, rubble cover -----------------
    s.rockA.setMeshData((makeSphere(0.9f) - translated(makeSphere(0.55f), {0.4f, 0.35f, 0.2f}))
                        ->buildMeshData(14, {1.f, 1.f, 1.f}, SolidUv::Spherical));
    s.rockB.setMeshData((makeSphere(0.7f) - translated(makeSphere(0.5f), {-0.3f, 0.3f, -0.25f})
                         - translated(makeSphere(0.35f), {0.1f, -0.3f, 0.3f}))
                        ->buildMeshData(14, {1.f, 1.f, 1.f}, SolidUv::Spherical));
    s.stalactite.setMeshData(buildParts({{makeCone(0.45f, 2.4f), fuhgl::math::rotateX(fuhgl::math::kPi), 10},
                                         {makeCone(0.25f, 1.4f), fuhgl::math::translate({0.3f, 0.5f, 0.2f}), 8}},
                                        {1.f, 1.f, 1.f}));
    s.stalagmite.setMeshData(buildParts({{makeCone(0.4f, 1.8f), Mat4{}, 10},
                                         {makeCone(0.22f, 1.0f), fuhgl::math::translate({-0.25f, 0.2f, 0.15f}), 8}},
                                        {1.f, 1.f, 1.f}));

    // --- mine set dressing: supports, rails + sleepers, cart, barrels, crystals ----------
    const Mat4 I{};
    s.support.setMeshData(buildParts({
        {makeCube({0.09f, 1.7f, 0.09f}), fuhgl::math::translate({-0.95f, 1.7f, 0.f}) * fuhgl::math::rotateZ(0.12f), 8},
        {makeCube({0.09f, 1.7f, 0.09f}), fuhgl::math::translate({0.95f, 1.7f, 0.f}) * fuhgl::math::rotateZ(-0.12f), 8},
        {makeCube({1.15f, 0.1f, 0.1f}), fuhgl::math::translate({0.f, 3.15f, 0.f}), 8},
        {makeCube({0.16f, 0.08f, 0.16f}), fuhgl::math::translate({-0.95f, 0.04f, 0.f}) * fuhgl::math::rotateZ(0.12f), 8},
        {makeCube({0.16f, 0.08f, 0.16f}), fuhgl::math::translate({0.95f, 0.04f, 0.f}) * fuhgl::math::rotateZ(-0.12f), 8},
    }, {1.f, 1.f, 1.f}));
    s.sleeper.setMeshData(buildParts({{makeCube({0.75f, 0.045f, 0.13f}), I, 8}}, {1.f, 1.f, 1.f}));
    s.rail.setMeshData(buildParts({{makeCube({0.05f, 0.06f, 15.f}), I, 8}}, {1.f, 1.f, 1.f}));
    s.cart.setMeshData(buildParts({
        {makeCube({0.65f, 0.32f, 1.0f}), fuhgl::math::translate({0.f, 0.42f, 0.f}), 8},
        {makeCube({0.7f, 0.06f, 1.05f}), fuhgl::math::translate({0.f, 0.24f, 0.f}), 8},
        {makeCylinder(0.17f, 0.06f), fuhgl::math::translate({-0.35f, 0.17f, 0.55f}) * fuhgl::math::rotateZ(fuhgl::math::kPi / 2.f), 10},
        {makeCylinder(0.17f, 0.06f), fuhgl::math::translate({0.35f, 0.17f, 0.55f}) * fuhgl::math::rotateZ(fuhgl::math::kPi / 2.f), 10},
        {makeCylinder(0.17f, 0.06f), fuhgl::math::translate({-0.35f, 0.17f, -0.55f}) * fuhgl::math::rotateZ(fuhgl::math::kPi / 2.f), 10},
        {makeCylinder(0.17f, 0.06f), fuhgl::math::translate({0.35f, 0.17f, -0.55f}) * fuhgl::math::rotateZ(fuhgl::math::kPi / 2.f), 10},
    }, {1.f, 1.f, 1.f}));
    s.barrel.setMeshData(buildParts({
        {makeCylinder(0.26f, 0.68f), I, 12},
        {makeTorus(0.27f, 0.025f), fuhgl::math::translate({0.f, 0.18f, 0.f}) * fuhgl::math::rotateX(fuhgl::math::kPi / 2.f), 10},
        {makeTorus(0.27f, 0.025f), fuhgl::math::translate({0.f, -0.18f, 0.f}) * fuhgl::math::rotateX(fuhgl::math::kPi / 2.f), 10},
    }, {1.f, 1.f, 1.f}));
    s.crystal.setMeshData(buildParts({
        {makeCone(0.12f, 0.75f), fuhgl::math::rotateZ(-0.15f), 10},
        {makeCone(0.09f, 0.55f), fuhgl::math::translate({0.14f, 0.1f, 0.05f}) * fuhgl::math::rotateZ(0.2f), 8},
        {makeCone(0.07f, 0.4f), fuhgl::math::translate({-0.12f, 0.06f, -0.06f}) * fuhgl::math::rotateZ(-0.3f), 8},
    }, {1.f, 1.f, 1.f}));
    s.puddle.setMeshData(buildParts({{makeCube({2.6f, 0.015f, 1.8f}), I, 8}}, {1.f, 1.f, 1.f}));
    s.skylight.setMeshData(xformMeshData(shapes::quadData(2.2f, {1.f, 1.f, 1.f}),
                                         fuhgl::math::rotateX(fuhgl::math::kPi / 2.f)));
    s.torchPole.setMeshData(buildParts({
        {makeCube({0.05f, 0.95f, 0.05f}), fuhgl::math::translate({0.f, 0.95f, 0.f}), 8},
        {makeCylinder(0.09f, 0.1f), fuhgl::math::translate({0.f, 1.92f, 0.f}), 8},
    }, {1.f, 1.f, 1.f}));
    s.flame.setMeshData(buildParts({{makeCone(0.07f, 0.24f), fuhgl::math::translate({0.f, 0.12f, 0.f}), 8},
                                    {makeSphere(0.06f), fuhgl::math::translate({0.f, 0.08f, 0.f}), 8}},
                                   {1.f, 1.f, 1.f}));

    // --- the knight (CSG parts merged per material) --------------------------------------
    const auto T = [](float x, float y, float z) { return fuhgl::math::translate({x, y, z}); };
    const auto RX = [](float a) { return fuhgl::math::rotateX(a); };
    const auto RY = [](float a) { return fuhgl::math::rotateY(a); };
    const auto RZ = [](float a) { return fuhgl::math::rotateZ(a); };
    s.knightArmor.setMeshData(buildParts({
        {makeCylinder(0.085f, 0.85f), T(-0.13f, 0.425f, 0.f) * RZ(0.06f), 12},                     // legs
        {makeCylinder(0.085f, 0.85f), T(0.13f, 0.425f, 0.f) * RZ(-0.06f), 12},
        {makeCube({0.13f, 0.09f, 0.21f}), T(-0.13f, 0.045f, 0.03f), 8},                             // boots
        {makeCube({0.13f, 0.09f, 0.21f}), T(0.13f, 0.045f, 0.03f), 8},
        {makeCube({0.27f, 0.34f, 0.19f}) & makeSphere(0.42f), T(0.f, 1.12f, 0.f), 14},             // cuirass
        {makeCylinder(0.24f, 0.09f), T(0.f, 0.8f, 0.f), 12},                                        // belt
        {makeSphere(0.17f), T(-0.34f, 1.42f, 0.f), 12},                                             // pauldrons
        {makeSphere(0.17f), T(0.34f, 1.42f, 0.f), 12},
        {makeCylinder(0.06f, 0.5f), T(-0.36f, 1.1f, 0.06f) * RZ(0.45f), 10},                        // arms
        {makeCylinder(0.06f, 0.5f), T(0.36f, 1.1f, 0.06f) * RZ(-0.5f), 10},
        {makeSphere(0.085f), T(-0.43f, 0.9f, 0.26f), 10},                                           // gauntlets
        {makeSphere(0.085f), T(0.44f, 0.86f, 0.3f), 10},
        {makeSphere(0.19f) - translated(makeCube({0.3f, 0.035f, 0.13f}), {0.f, 0.02f, 0.17f})        // helmet with visor slit
                         - translated(makeCube({0.035f, 0.16f, 0.13f}), {0.f, 0.02f, 0.17f}),
         T(0.f, 1.72f, 0.f), 14},
    }, {1.f, 1.f, 1.f}));
    s.knightBlade.setMeshData(buildParts({
        {makeCube({0.025f, 0.55f, 0.07f}), T(0.44f, 0.62f, 0.42f) * RX(0.85f), 8},                // blade
        {makeCube({0.1f, 0.02f, 0.13f}), T(0.44f, 0.86f, 0.3f) * RX(0.85f), 8},                    // guard
        {makeCylinder(0.02f, 0.15f), T(0.44f, 0.93f, 0.24f) * RX(0.85f), 8},                       // hilt
        {makeSphere(0.035f), T(0.44f, 1.0f, 0.19f) * RX(0.85f), 8},                                // pommel
    }, {1.f, 1.f, 1.f}));
    s.knightShield.setMeshData(buildParts({
        {makeCylinder(0.27f, 0.045f), T(-0.46f, 1.05f, 0.28f) * RX(fuhgl::math::kPi / 2.f), 14},   // shield
        {makeSphere(0.06f), T(-0.46f, 1.05f, 0.33f), 10},                                           // boss
    }, {1.f, 1.f, 1.f}));
    s.knightPlume.setMeshData(buildParts({{makeCone(0.05f, 0.28f), T(0.f, 1.98f, -0.03f) * RX(-0.35f), 8}},
                                    {1.f, 1.f, 1.f}));
    s.lantern.setMeshData(buildParts({{makeCube({0.07f, 0.1f, 0.07f}), T(0.2f, 0.72f, 0.14f), 8}},
                                 {1.f, 1.f, 1.f}));

    // --- one zombie (merged CSG parts, arms stretched forward); instanced as a horde -----
    s.zombieBody.setMeshData(buildParts({
        {makeCylinder(0.07f, 0.8f), T(-0.11f, 0.4f, 0.f) * RZ(0.05f), 10},                         // legs
        {makeCylinder(0.07f, 0.8f), T(0.11f, 0.4f, 0.f) * RZ(-0.08f), 10},
        {makeCube({0.24f, 0.32f, 0.15f}) & makeSphere(0.36f), T(0.f, 0.95f, 0.02f) * RX(0.22f), 12}, // hunched torso
        {makeSphere(0.16f) - translated(makeCube({0.2f, 0.07f, 0.15f}), {0.f, -0.06f, 0.1f}),       // skull with open jaw
         T(0.f, 1.38f, 0.1f) * RX(0.28f), 12},
        {makeCylinder(0.055f, 0.62f), T(-0.28f, 1.12f, 0.28f) * RX(-1.25f), 10},                   // arms stretched
        {makeCylinder(0.055f, 0.62f), T(0.28f, 1.12f, 0.28f) * RX(-1.25f), 10},
        {makeSphere(0.07f), T(-0.28f, 1.2f, 0.78f) * RX(-1.25f), 8},                               // hands
        {makeSphere(0.07f), T(0.28f, 1.2f, 0.78f) * RX(-1.25f), 8},
    }, {1.f, 1.f, 1.f}));
    s.zombieEyes.setMeshData(buildParts({
        {makeSphere(0.045f), T(-0.06f, 1.42f, 0.22f), 8},
        {makeSphere(0.045f), T(0.06f, 1.42f, 0.22f), 8},
    }, {1.f, 1.f, 1.f}));

    // --- scene assembly -------------------------------------------------------------------
    s.scene.camera = &cam_;
    s.scene.width = width;
    s.scene.height = height;
    s.scene.aspect = static_cast<float>(width) / static_cast<float>(height);
    s.scene.clearColor = {0.016f, 0.022f, 0.03f};
    s.scene.ambient = {0.02f, 0.026f, 0.038f};
    s.scene.fogColor = {0.016f, 0.022f, 0.03f};
    s.scene.fogDensity = 0.03f;

    s.scene.items.push_back({&s.cavern, &s.cavernMat, Mat4{}});
    s.scene.items.push_back({nullptr, &s.floorMat, Mat4{}, &s.floorLod}); // F41 LOD floor
    s.scene.items.push_back({&s.puddle, &s.puddleMat, fuhgl::math::translate({2.8f, floorH(2.8f, -8.f) + 0.02f, -8.f})});
    s.scene.items.push_back({&s.skylight, &s.skylightMat, fuhgl::math::translate({6.f, 10.6f, -10.f})});

    // rocks: scattered rubble + explicit cover rocks in front of the ambush spots
    std::vector<Mat4> rocksA, rocksB;
    // ambush spots as (x, z): a zombie lurks at each, hidden behind a cover rock; the two
    // near spots flank the knight so the horde is already visible in the wide shot
    const Vec3 ambush[6] = {{-7.5f, 0.f, -8.5f}, {8.f, 0.f, -10.f}, {-4.f, 0.f, -14.f},
                            {5.f, 0.f, -16.f}, {-12.f, 0.f, -13.f}, {11.f, 0.f, -17.f}};
    for (int i = 0; i < 26; ++i) {
        const float x = (hash01(i, 1, 15) - 0.5f) * 54.f;
        const float z = (hash01(i, 2, 16) - 0.5f) * 46.f - 4.f;
        if (std::sqrt(x * x + (z + 3.f) * (z + 3.f)) < 3.f) continue;
        const float sc = 0.4f + hash01(i, 3, 17) * 1.3f;
        const Mat4 m = fuhgl::math::translate({x, floorH(x, z) + 0.05f * sc, z}) *
                       RY(hash01(i, 4, 18) * 6.2831853f) *
                       fuhgl::math::scale({sc, sc * (0.7f + 0.4f * hash01(i, 5, 19)), sc});
        (i % 2 == 0 ? rocksA : rocksB).push_back(m);
    }
    for (int i = 0; i < 6; ++i) { // low cover rock beside/in front of each lurking zombie
        const float x = ambush[i].x + (hash01(i, 6, 20) - 0.5f) * 2.2f, z = ambush[i].z + 1.9f;
        const float sc = 0.85f + hash01(i, 7, 21) * 0.35f;
        rocksA.push_back(fuhgl::math::translate({x, floorH(x, z) - 0.15f, z}) * RY(hash01(i, 8, 22) * 6.28f) *
                         fuhgl::math::scale({sc, sc * 0.7f, sc}));
    }
    s.rockA.setInstanceData(rocksA.data(), static_cast<fuhgl::gl::GLsizei>(rocksA.size()));
    s.rockB.setInstanceData(rocksB.data(), static_cast<fuhgl::gl::GLsizei>(rocksB.size()));
    s.scene.items.push_back({&s.rockA, &s.rockMat, Mat4{}});
    s.scene.items.push_back({&s.rockB, &s.rockMat, Mat4{}});

    // stalactites on the ceiling + stalagmites on the floor (one instanced draw each)
    std::vector<Mat4> stal, stalag;
    for (int i = 0; i < 60; ++i) {
        const float x = (hash01(i, 11, 61) - 0.5f) * 56.f;
        const float z = (hash01(i, 12, 62) - 0.5f) * 46.f - 4.f;
        const float ex = std::max(std::abs(x) / 32.f, std::abs(z) / 28.f);
        const float ceilY = -2.f + 14.f * std::sqrt(std::max(0.05f, 1.f - (x * x) / (32.f * 32.f) - (z * z) / (28.f * 28.f)));
        if (ex > 0.95f) continue;
        const float sc = 0.5f + hash01(i, 13, 63) * 1.1f;
        stal.push_back(fuhgl::math::translate({x, ceilY - 0.1f, z}) * RY(hash01(i, 14, 64) * 6.28f) *
                       fuhgl::math::rotateZ((hash01(i, 15, 65) - 0.5f) * 0.2f) * fuhgl::math::scale({sc, sc, sc}));
    }
    for (int i = 0; i < 22; ++i) {
        const float x = (hash01(i, 16, 66) - 0.5f) * 50.f;
        const float z = (hash01(i, 17, 67) - 0.5f) * 44.f - 4.f;
        if (std::sqrt(x * x + (z + 3.f) * (z + 3.f)) < 2.5f) continue;
        const float sc = 0.4f + hash01(i, 18, 68) * 1.0f;
        stalag.push_back(fuhgl::math::translate({x, floorH(x, z) - 0.05f, z}) * RY(hash01(i, 19, 69) * 6.28f) *
                         fuhgl::math::scale({sc, sc, sc}));
    }
    s.stalactite.setInstanceData(stal.data(), static_cast<fuhgl::gl::GLsizei>(stal.size()));
    s.stalagmite.setInstanceData(stalag.data(), static_cast<fuhgl::gl::GLsizei>(stalag.size()));
    s.scene.items.push_back({&s.stalactite, &s.rockMat, Mat4{}});
    s.scene.items.push_back({&s.stalagmite, &s.rockMat, Mat4{}});

    // timber supports along the corridor, rails + sleepers, mine cart, barrels, crystals
    std::vector<Mat4> supports, sleepers, barrels, crystals;
    for (int i = 0; i < 4; ++i) {
        const float z = 3.f - i * 8.f;
        const float y = floorH(0.f, z);
        supports.push_back(fuhgl::math::translate({-2.4f, y - 0.05f, z}) * RY(0.06f * i));
        supports.push_back(fuhgl::math::translate({2.4f, y - 0.05f, z}) * RY(-0.05f * i));
    }
    for (int i = 0; i < 24; ++i) {
        const float z = 6.f - i * 1.15f;
        sleepers.push_back(fuhgl::math::translate({0.f, floorH(0.f, z) + 0.06f, z}));
    }
    s.support.setInstanceData(supports.data(), static_cast<fuhgl::gl::GLsizei>(supports.size()));
    s.sleeper.setInstanceData(sleepers.data(), static_cast<fuhgl::gl::GLsizei>(sleepers.size()));
    s.scene.items.push_back({&s.support, &s.woodMat, Mat4{}});
    s.scene.items.push_back({&s.sleeper, &s.woodMat, Mat4{}});
    const float railY = floorH(0.f, -9.f) + 0.14f;
    s.scene.items.push_back({&s.rail, &s.ironMat, fuhgl::math::translate({-0.55f, railY, -9.f})});
    s.scene.items.push_back({&s.rail, &s.ironMat, fuhgl::math::translate({0.55f, railY, -9.f})});
    s.scene.items.push_back({&s.cart, &s.woodMat,
                             fuhgl::math::translate({0.55f, floorH(0.55f, -17.f), -17.f}) * RZ(0.05f)});
    for (int i = 0; i < 6; ++i) {
        const float x = (hash01(i, 21, 71) - 0.5f) * 40.f;
        const float z = (hash01(i, 22, 72) - 0.5f) * 40.f - 2.f;
        if (std::sqrt(x * x + (z + 3.f) * (z + 3.f)) < 3.f) continue;
        barrels.push_back(fuhgl::math::translate({x, floorH(x, z), z}) * RY(hash01(i, 23, 73) * 6.28f));
    }
    s.barrel.setInstanceData(barrels.data(), static_cast<fuhgl::gl::GLsizei>(barrels.size()));
    s.scene.items.push_back({&s.barrel, &s.woodMat, Mat4{}});
    const Vec3 crystalSpots[5] = {{-24.f, 0.f, -6.f}, {22.f, 0.f, -18.f}, {-18.f, 0.f, 12.f},
                                  {25.f, 0.f, 6.f}, {10.f, 0.f, 18.f}};
    for (const Vec3& c : crystalSpots) {
        crystals.push_back(fuhgl::math::translate({c.x, floorH(c.x, c.z) + 0.05f, c.z}) *
                           RY(hash01(static_cast<int>(c.x), static_cast<int>(c.z), 74) * 6.28f) *
                           fuhgl::math::scale({1.2f, 1.6f, 1.2f}));
    }
    s.crystal.setInstanceData(crystals.data(), static_cast<fuhgl::gl::GLsizei>(crystals.size()));
    s.scene.items.push_back({&s.crystal, &s.crystalMat, Mat4{}});

    // torches: instanced poles + animated instanced flames + flickering point lights
    const Vec3 torches[6] = {{-14.f, 0.f, -6.f}, {12.f, 0.f, 4.f}, {-4.f, 0.f, -18.f},
                             {16.f, 0.f, -14.f}, {-18.f, 0.f, 8.f}, {7.f, 0.f, 12.f}};
    std::vector<Mat4> poles;
    s.flameBase.clear();
    for (int i = 0; i < 6; ++i) {
        const float x = torches[i].x, z = torches[i].z;
        const float y = floorH(x, z);
        poles.push_back(fuhgl::math::translate({x, y, z}));
        s.flameBase.push_back(fuhgl::math::translate({x, y + 2.02f, z}));
        s.torchBaseIntensity.push_back(9.f + hash01(i, 24, 75) * 3.f);
        s.torchLightIndices.push_back(-1); // filled below
    }
    s.torchPole.setInstanceData(poles.data(), static_cast<fuhgl::gl::GLsizei>(poles.size()));
    s.flame.setInstanceData(s.flameBase.data(), static_cast<fuhgl::gl::GLsizei>(s.flameBase.size()));
    s.scene.items.push_back({&s.torchPole, &s.woodMat, Mat4{}});
    s.scene.items.push_back({&s.flame, &s.flameMat, Mat4{}});

    // the knight, lantern included, facing the camera (+Z)
    s.knightPos = {0.f, 0.f, -3.f};
    s.knightPos.y = floorH(s.knightPos.x, s.knightPos.z);
    const Vec3& knightPos = s.knightPos;
    const Mat4 knightRoot = fuhgl::math::translate(knightPos);
    s.scene.items.push_back({&s.knightArmor, &s.armorMat, knightRoot});
    s.scene.items.push_back({&s.knightBlade, &s.bladeMat, knightRoot});
    s.scene.items.push_back({&s.knightShield, &s.shieldMat, knightRoot});
    s.scene.items.push_back({&s.knightPlume, &s.plumeMat, knightRoot});
    s.scene.items.push_back({&s.lantern, &s.lanternMat, knightRoot});

    // zombie horde lurking in the dark (instanced body + emissive eyes, animated per frame)
    s.zombieRotScale.clear();
    s.zombiePos.clear();
    for (int i = 0; i < 6; ++i) {
        const float x = ambush[i].x, z = ambush[i].z;
        const float y = floorH(x, z);
        s.zombiePos.push_back({x, y, z});
        const float yaw = hash01(i, 25, 76) * 0.8f - 0.4f; // roughly facing the knight
        const float sc = 0.95f + hash01(i, 26, 77) * 0.25f;
        s.zombieRotScale.push_back(RY(yaw) * fuhgl::math::scale({sc, sc, sc}));
    }
    s.zombieModels.assign(s.zombiePos.size(), Mat4{});
    s.zombieBody.setInstanceData(s.zombieModels.data(), static_cast<fuhgl::gl::GLsizei>(s.zombieModels.size()));
    s.zombieEyes.setInstanceData(s.zombieModels.data(), static_cast<fuhgl::gl::GLsizei>(s.zombieModels.size()));
    s.scene.items.push_back({&s.zombieBody, &s.zombieMat, Mat4{}});
    s.scene.items.push_back({&s.zombieEyes, &s.eyeMat, Mat4{}});

    // --- lights: moon shaft through the ceiling hole first (drives god rays + CSM) -------
    s.scene.lights.push_back(Light::directional(fuhgl::math::normalize(Vec3{-0.22f, -0.97f, 0.13f}),
                                                {0.5f, 0.66f, 1.0f}, 2.2f));
    s.scene.lights.push_back(Light::point({knightPos.x + 0.2f, knightPos.y + 0.75f, knightPos.z + 0.14f},
                                          {1.f, 0.68f, 0.32f}, 7.f, 11.f)); // knight lantern
    for (int i = 0; i < 6; ++i) {
        const float x = torches[i].x, z = torches[i].z;
        s.torchLightIndices[i] = static_cast<int>(s.scene.lights.size());
        s.scene.lights.push_back(Light::point({x, floorH(x, z) + 2.1f, z},
                                              {1.f, 0.5f, 0.18f}, s.torchBaseIntensity[i], 13.f));
    }
    for (size_t i = 0; i < s.zombiePos.size(); ++i) // eerie green glow at every ambush spot
        s.scene.lights.push_back(Light::point({s.zombiePos[i].x, s.zombiePos[i].y + 1.4f, s.zombiePos[i].z},
                                              {0.35f, 1.f, 0.3f}, 3.f, 9.f));

    // --- IBL from a dark procedural cubemap (F29) ----------------------------------------
    s.sky.create();
    s.sky.makeGradient({0.004f, 0.008f, 0.02f}, {0.02f, 0.03f, 0.05f}, 64);
    if (!s.env.createFromCubemap(s.sky.texture())) {
        std::fprintf(stderr, "mine: IBL bake failed\n");
        return false;
    }
    s.env.intensity = 0.7f;
    s.scene.env = &s.env;

    // --- camera per mode ------------------------------------------------------------------
    const auto lookAt = [&](const Vec3& target) {
        const Vec3 d = target - cam_.position;
        const float len = fuhgl::math::length(d);
        cam_.pitch = std::asin(fuhgl::math::clamp(d.y / len, -1.f, 1.f));
        cam_.yaw = std::atan2(d.x, -d.z);
    };
    Vec3 focusTarget = knightPos + Vec3{0.f, 1.4f, 0.f};
    if (mode == "knight") {
        cam_.position = {2.4f, floorH(2.4f, 0.8f) + 1.62f, 0.8f};
        lookAt({knightPos.x, knightPos.y + 1.45f, knightPos.z});
    } else if (mode == "horde") {
        cam_.position = {-6.5f, floorH(-6.5f, 1.5f) + 1.75f, 1.5f};
        focusTarget = {-9.f, floorH(-9.f, -13.f) + 1.2f, -13.f};
        lookAt(focusTarget);
    } else if (mode == "probe") { // right in front of the first lurking zombie
        cam_.position = {-9.f, floorH(-9.f, -9.5f) + 1.6f, -9.5f};
        focusTarget = {-9.f, floorH(-9.f, -13.f) + 1.3f, -13.f};
        lookAt(focusTarget);
    } else {
        cam_.position = {0.f, floorH(0.f, 8.f) + 1.8f, 8.f};
        cam_.yaw = 0.f;
        cam_.pitch = -0.12f;
    }
    s.knightDist = fuhgl::math::length(focusTarget - cam_.position);
    s.renderer.dofEnabled = true; // F36: focus on the subject of the mode
    s.renderer.dofFocusDist = s.knightDist;
    s.renderer.dofFocusRange = 3.5f;
    s.renderer.dofStrength = 6.f;
    s.renderer.dofTaps = 8;

    if (!s.text.create()) {
        std::fprintf(stderr, "mine: TextRenderer create failed\n");
        return false;
    }
    if (!s.wire.create()) {
        std::fprintf(stderr, "mine: Wireframe create failed\n");
        return false;
    }
    return true;
}

void HauntedMine::renderFrame(float time) {
    Impl& s = *impl_;
    // lurking sway of the horde: slow yaw + forward lean pulse per zombie
    for (size_t i = 0; i < s.zombieRotScale.size(); ++i) {
        const Vec3 p = s.zombiePos[i];
        const float sway = 0.07f * std::sin(time * 0.9f + static_cast<float>(i) * 1.7f);
        const float lean = 0.05f + 0.03f * std::sin(time * 1.3f + static_cast<float>(i) * 2.3f);
        s.zombieModels[i] = fuhgl::math::translate(p) * fuhgl::math::rotateY(sway) *
                            fuhgl::math::rotateX(lean) * s.zombieRotScale[i];
    }
    s.zombieBody.updateInstanceData(s.zombieModels.data(), static_cast<fuhgl::gl::GLsizei>(s.zombieModels.size()));
    s.zombieEyes.updateInstanceData(s.zombieModels.data(), static_cast<fuhgl::gl::GLsizei>(s.zombieModels.size()));
    // torch flames flicker (scale) + lights flicker (intensity)
    s.flameModels = s.flameBase;
    for (size_t i = 0; i < s.flameBase.size(); ++i) {
        const float f = 0.85f + 0.3f * fuhgl::math::fbm2(time * 3.f + static_cast<float>(i) * 7.f, 0.f, 2, 91u);
        const Vec3 p{s.flameBase[i].col(3).x, s.flameBase[i].col(3).y, s.flameBase[i].col(3).z};
        s.flameModels[i] = fuhgl::math::translate(p) * fuhgl::math::scale({f, 0.8f + 0.5f * f, f});
        const int li = s.torchLightIndices[i];
        if (li >= 0)
            s.scene.lights[static_cast<size_t>(li)].intensity =
                s.torchBaseIntensity[i] * (0.8f + 0.35f * f);
    }
    s.flame.updateInstanceData(s.flameModels.data(), static_cast<fuhgl::gl::GLsizei>(s.flameModels.size()));

    s.renderer.render(s.scene);
}

void HauntedMine::drawHud(float fps) {
    Impl& s = *impl_;
    s.renderer.output().bind();
    if (wireframe_) {
        s.wire.clear();
        for (const Vec3& p : s.zombiePos)
            s.wire.addBox({p.x - 0.55f, p.y, p.z - 0.55f}, {p.x + 0.55f, p.y + 1.8f, p.z + 0.55f},
                          {1.f, 0.15f, 0.1f});
        s.wire.addBox({s.knightPos.x - 0.55f, s.knightPos.y, s.knightPos.z - 0.6f},
                      {s.knightPos.x + 0.55f, s.knightPos.y + 2.f, s.knightPos.z + 0.6f}, {1.f, 1.f, 1.f});
        s.wire.render(cam_, width_, height_);
    }
    char line[512];
    s.text.begin(width_, height_);
    s.text.draw("FuhGLCore - HAUNTED MINE  (deferred: CSM+SSAO+HDR+bloom+god rays+TAA+SSR+DoF+vol fog | PBR+CSG+instancing+terrain LOD+IBL)",
                12.f, 10.f, 1.5f, {0.9f, 0.85f, 0.7f, 1.f});
    std::snprintf(line, sizeof(line),
                  "mode=%s  FPS: %.1f   WASD+mouse move | Esc quit | V wireframe | B/G/O/T/D/F/S/C/I/L toggle effects",
                  mode_.c_str(), fps);
    s.text.draw(line, 12.f, 32.f, 1.f, {0.75f, 0.78f, 0.85f, 1.f});
    std::snprintf(line, sizeof(line),
                  "bloom[%s] godrays[%s] ssao[%s] taa[%s] dof[%s] volfog[%s] ssr[%s] csm[%s] ibl[%s] lod[%s] wire[%s]",
                  effectOn(Effect::Bloom) ? "on" : "off", effectOn(Effect::GodRays) ? "on" : "off",
                  effectOn(Effect::Ssao) ? "on" : "off", effectOn(Effect::Taa) ? "on" : "off",
                  effectOn(Effect::DoF) ? "on" : "off", effectOn(Effect::VolFog) ? "on" : "off",
                  effectOn(Effect::Ssr) ? "on" : "off", effectOn(Effect::CsmShadows) ? "on" : "off",
                  effectOn(Effect::Ibl) ? "on" : "off", effectOn(Effect::Lod) ? "on" : "off",
                  wireframe_ ? "on" : "off");
    s.text.draw(line, 12.f, 46.f, 1.f, {0.6f, 0.9f, 0.6f, 1.f});
    s.text.end();
}

void HauntedMine::toggleEffect(fuhgl::render::Effect e) {
    Impl& s = *impl_;
    const bool on = !effectOn(e);
    s.renderer.setEffectEnabled(e, on);
    if (e == fuhgl::render::Effect::Ibl) {
        s.iblOn = on;
        s.scene.env = on ? &s.env : nullptr;
    }
    if (e == fuhgl::render::Effect::Lod) s.renderer.lodEnabled = on;
}

bool HauntedMine::effectOn(fuhgl::render::Effect e) const {
    const Impl& s = *impl_;
    if (e == fuhgl::render::Effect::Ibl) return s.iblOn;
    return s.renderer.effectEnabled(e);
}

fuhgl::Framebuffer& HauntedMine::output() { return impl_->renderer.output(); }

void HauntedMine::setDofFocus(float d) { impl_->renderer.dofFocusDist = d; }

void HauntedMine::resize(int w, int h) {
    if (w == width_ && h == height_ || w <= 0 || h <= 0) return;
    width_ = w;
    height_ = h;
    impl_->renderer.resize(w, h);
    impl_->scene.width = w;
    impl_->scene.height = h;
    impl_->scene.aspect = static_cast<float>(w) / static_cast<float>(h);
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
    if (!window.create(headless ? "mine-screenshot" : "FuhGLCore - haunted mine",
                       args.width, args.height)) {
        std::fprintf(stderr, "mine: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "mine: need GL >= 3.3\n");
        return 2;
    }

    HauntedMine mine;
    if (!mine.create(args.width, args.height, args.mode)) return 1;
    if (args.focus >= 0.f) mine.setDofFocus(args.focus);
    if (args.nodof) mine.toggleEffect(Effect::DoF);
    if (args.wire) mine.toggleWireframe();

    if (headless) {
        for (int i = 0; i < args.frames; ++i)
            mine.renderFrame(static_cast<float>(i) / 30.f); // deterministic time, TAA accumulates
        fuhgl::gl::flush();
        mine.output().bind();
        mine.drawHud(60.0f);
        fuhgl::gl::flush();
        std::vector<unsigned char> px(static_cast<size_t>(args.width) * args.height * 4);
        fuhgl::gl::readPixels(0, 0, args.width, args.height, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        std::vector<unsigned char> flipped(px.size());
        const size_t stride = static_cast<size_t>(args.width) * 4;
        for (int y = 0; y < args.height; ++y)
            std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                        px.data() + static_cast<size_t>(args.height - 1 - y) * stride, stride);
        if (!screenshot::writePng(args.screenshot.c_str(), flipped.data(), args.width, args.height)) {
            std::fprintf(stderr, "mine: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "mine: wrote %s (mode=%s, %dx%d, %d frames, GL %d.%d)\n",
                     args.screenshot.c_str(), args.mode.c_str(), args.width, args.height, args.frames,
                     fuhgl::gl::version().major, fuhgl::gl::version().minor);
        return 0;
    }

    mine.camera().attach(window);
    window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
        if (action != fuhgl::Action::Press) {
            mine.camera().onKey(key, action);
            return;
        }
        mine.camera().onKey(key, action);
        switch (key) {
            case fuhgl::Key::Escape: window.setShouldClose(true); break;
            case fuhgl::Key::V: mine.toggleWireframe(); break;
            case fuhgl::Key::B: mine.toggleEffect(Effect::Bloom); break;
            case fuhgl::Key::G: mine.toggleEffect(Effect::GodRays); break;
            case fuhgl::Key::O: mine.toggleEffect(Effect::Ssao); break;
            case fuhgl::Key::T: mine.toggleEffect(Effect::Taa); break;
            case fuhgl::Key::D: mine.toggleEffect(Effect::DoF); break;
            case fuhgl::Key::F: mine.toggleEffect(Effect::VolFog); break;
            case fuhgl::Key::S: mine.toggleEffect(Effect::Ssr); break;
            case fuhgl::Key::C: mine.toggleEffect(Effect::CsmShadows); break;
            case fuhgl::Key::I: mine.toggleEffect(Effect::Ibl); break;
            case fuhgl::Key::L: mine.toggleEffect(Effect::Lod); break;
            default: break;
        }
    });
    fuhgl::render::FpsCounter fps;
    double lastTime = window.time();
    while (!window.shouldClose()) {
        window.pollEvents();
        const double now = window.time();
        fps.frame(now);
        mine.camera().update(static_cast<float>(now - lastTime));
        lastTime = now;
        mine.resize(window.framebufferWidth(), window.framebufferHeight());
        mine.renderFrame(static_cast<float>(now));
        mine.drawHud(fps.fps());
        fuhgl::Framebuffer::blitToDefault(mine.output(), mine.width(), mine.height());
        window.swapBuffers();
    }
    return 0;
}
