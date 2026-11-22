// README showcase (F25): an organic landscape rendered with the DeferredRenderer and its
// full effect stack — cascaded shadow maps (F30), SSAO, HDR lighting with a visible sun
// disk, bloom, crepuscular god rays, distance fog, TAA (F32), SSR (F33), depth of field
// (F36) and volumetric fog / light shafts (F37), plus a gradient sky. The terrain is a
// noise heightfield; CSG solids form pine/broadleaf trees, bushes, rocks and a monolith;
// grass is one instanced mesh (F24); a pond reflects the sky via SSR (F33).
// Three looks: --mode day | --mode sunset (sun low in frame -> long shadows + strong god
// rays + warm fog shafts) | --mode night (dark sky, IBL environment, emissive lamps
// reflected via SSR). CI renders all three as README screenshots.
//
// With --screenshot a few deterministic frames are rendered headless (surfaceless EGL,
// TAA history accumulates) into the renderer output and written as PNG.
//
// Usage: showcase [--screenshot out.png] [--mode day|sunset|night] [--width W] [--height H]
// Controls (windowed): WASD+mouse, Esc quits.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include "../screenshot/png_writer.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Args {
    std::string screenshot;
    std::string mode = "day";
    int width = 960, height = 540;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) a.screenshot = argv[++i];
        else if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) a.mode = argv[++i];
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) a.width = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) a.height = std::atoi(argv[++i]);
    }
    return a;
}

float hash01(int i, int j, int seed) {
    const float s = std::sin(i * 12.9898f + j * 78.233f + seed * 37.719f) * 43758.5453f;
    return s - std::floor(s);
}

} // namespace

int main(int argc, char** argv) {
    const Args args = parseArgs(argc, argv);
    const bool sunset = args.mode == "sunset";
    const bool night = args.mode == "night";
    const bool headless = !args.screenshot.empty();
    if (headless) {
#ifndef _WIN32
        if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
        if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif
    }

    fuhgl::Window window;
    if (!window.create(headless ? "showcase-screenshot" : "FuhGLCore - showcase",
                       args.width, args.height)) {
        std::fprintf(stderr, "showcase: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "showcase: need GL >= 3.3\n");
        return 2;
    }

    using namespace fuhgl::render;

    // --- terrain height function (shared by the mesh, placement and the camera) ---------
    const auto terrainH = [](float x, float z) {
        const float d = std::sqrt(x * x + z * z);
        float h = 2.6f * fuhgl::math::fbm2(x * 0.045f, z * 0.045f, 4, 7u)
                + 0.55f * fuhgl::math::fbm2(x * 0.17f, z * 0.17f, 3, 13u);
        h *= fuhgl::math::smoothstep(2.5f, 10.f, d); // clearing around the scene center
        const float pdx = x - 7.f, pdz = z + 9.f;     // pond basin at (7, -9)
        const float pd = std::sqrt(pdx * pdx + pdz * pdz);
        const float basin = fuhgl::math::smoothstep(7.f, 2.5f, pd);
        h = h * (1.f - basin) + (-1.3f) * basin;
        h -= 0.9f * std::exp(-(pdx * pdx + pdz * pdz) / 22.f);
        return h;
    };

    // --- renderer with the full effect stack -------------------------------------------
    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, terrainH(0.f, 16.f) + 2.3f, 16.f};
    cam.yaw = -0.2f;   // aim at the monolith / pond
    cam.pitch = -0.14f;

    DeferredRenderer renderer;
    if (!renderer.create(args.width, args.height)) {
        std::fprintf(stderr, "showcase: renderer create failed\n");
        return 1;
    }
    renderer.enableCsmShadows(2048, 4); // F30: cascaded shadow maps
    renderer.setShadowZRange(0.5f, 90.f);
    renderer.setShadowSceneBounds({0.f, 2.f, -4.f}, 24.f);
    renderer.setShadowStrength(1.f);
    renderer.ssaoEnabled = true;
    renderer.ssaoRadius = 0.8f;
    renderer.ssaoStrength = 1.1f;
    renderer.bloomStrength = night ? 1.0f : 0.8f;
    renderer.bloomThreshold = night ? 0.8f : 1.0f;
    renderer.godRaysEnabled = true;
    renderer.rayWeight = sunset ? 0.55f : 0.35f;
    renderer.exposure = 1.05f;
    renderer.taaEnabled = true;  // F32
    renderer.ssrEnabled = true;  // F33
    renderer.ssrStrength = 0.55f;
    renderer.ssrMaxSteps = 48;
    renderer.ssrThickness = 0.5f;
    // F36: depth of field focused on the monolith
    renderer.dofEnabled = true;
    renderer.dofFocusDist = fuhgl::math::length(fuhgl::math::Vec3{-4.5f, terrainH(-4.5f, -6.f) + 2.4f, -6.f} - cam.position);
    renderer.dofFocusRange = 7.f;
    renderer.dofStrength = 10.f;
    renderer.dofTaps = 20;
    // F37: volumetric fog — morning mist (day), warm haze (sunset), thin night fog
    renderer.volFogEnabled = true;
    renderer.volFogDensity = night ? 0.004f : sunset ? 0.008f : 0.006f;
    renderer.volFogColor = night ? fuhgl::math::Vec3{0.05f, 0.07f, 0.12f}
                           : sunset ? fuhgl::math::Vec3{0.85f, 0.55f, 0.40f}
                                     : fuhgl::math::Vec3{0.72f, 0.78f, 0.88f};
    renderer.volFogAnisotropy = sunset ? 0.6f : 0.45f;
    renderer.volFogHeight = 0.6f;
    renderer.volFogFalloff = 0.16f;
    renderer.volFogShadows = !night; // light shafts through the trees
    renderer.volFogSteps = 24;
    renderer.volFogMaxDist = 90.f;
    if (night) {
        renderer.pipeline().skyTop = {0.015f, 0.02f, 0.06f};
        renderer.pipeline().skyBottom = {0.06f, 0.08f, 0.16f};
    } else if (sunset) {
        renderer.pipeline().skyTop = {0.10f, 0.07f, 0.22f};
        renderer.pipeline().skyBottom = {0.95f, 0.42f, 0.16f};
    } else {
        renderer.pipeline().skyTop = {0.16f, 0.40f, 0.85f};
        renderer.pipeline().skyBottom = {0.72f, 0.80f, 0.92f};
    }

    // --- terrain: noise heightfield grid (normals by finite differences, +y winding) -----
    const float kTerrainR = 30.f;
    const int kTerrainN = 88;
    MeshData terrainData;
    {
        const float step = 2.f * kTerrainR / kTerrainN;
        terrainData.vertices.reserve(static_cast<size_t>(kTerrainN + 1) * (kTerrainN + 1) * MeshData::kFloatsPerVertex);
        for (int j = 0; j <= kTerrainN; ++j)
            for (int i = 0; i <= kTerrainN; ++i) {
                const float x = -kTerrainR + i * step, z = -kTerrainR + j * step;
                const float y = terrainH(x, z);
                const float e = step * 0.5f;
                const fuhgl::math::Vec3 n = fuhgl::math::normalize(
                    {terrainH(x - e, z) - terrainH(x + e, z), 2.f * e, terrainH(x, z - e) - terrainH(x, z + e)});
                const float g = 0.9f + 0.1f * fuhgl::math::fbm2(x * 0.3f, z * 0.3f, 2, 29u);
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
    }
    Mesh terrainMesh;
    terrainMesh.setMeshData(terrainData);
    fuhgl::Texture grassTex = procedural::makeTexture(procedural::noise(256, 256, 7u, 5));
    Material terrainMat = materials::pbrTextured(grassTex,
                                                 night ? fuhgl::math::Vec3{0.10f, 0.13f, 0.16f}
                                                 : sunset ? fuhgl::math::Vec3{0.38f, 0.30f, 0.16f}
                                                          : fuhgl::math::Vec3{0.30f, 0.50f, 0.22f},
                                                 0.f, 0.95f);

    // --- trees: CSG trunk + canopy, two species, scattered on the hills -------------------
    Mesh pineTrunk, pineCanopy, broadTrunk, broadCanopy;
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
    Material trunkMat = materials::matte(night ? fuhgl::math::Vec3{0.10f, 0.08f, 0.06f}
                                               : fuhgl::math::Vec3{0.30f, 0.21f, 0.13f});
    Material pineMat = materials::matte(night ? fuhgl::math::Vec3{0.05f, 0.10f, 0.07f}
                                    : sunset ? fuhgl::math::Vec3{0.16f, 0.22f, 0.10f}
                                             : fuhgl::math::Vec3{0.13f, 0.35f, 0.15f});
    Material broadMat = materials::matte(night ? fuhgl::math::Vec3{0.06f, 0.10f, 0.05f}
                                     : sunset ? fuhgl::math::Vec3{0.24f, 0.20f, 0.08f}
                                              : fuhgl::math::Vec3{0.22f, 0.42f, 0.16f});

    // --- bushes: CSG sphere blobs ------------------------------------------------------------
    auto bushSolidA = makeSphere(0.55f) - translated(makeSphere(0.38f), {0.3f, 0.25f, 0.15f});
    auto bushSolidB = makeSphere(0.45f) - translated(makeSphere(0.3f), {-0.25f, 0.2f, -0.15f});
    Mesh bushMeshA = bushSolidA->buildMesh(12, {1.f, 1.f, 1.f});
    Mesh bushMeshB = bushSolidB->buildMesh(12, {1.f, 1.f, 1.f});
    Material bushMat = materials::matte(night ? fuhgl::math::Vec3{0.05f, 0.09f, 0.05f}
                                    : sunset ? fuhgl::math::Vec3{0.20f, 0.18f, 0.08f}
                                             : fuhgl::math::Vec3{0.17f, 0.36f, 0.13f});

    // --- rocks: two CSG blobs with a voronoi texture -----------------------------------------
    auto rockSolidA = makeSphere(0.9f) - translated(makeSphere(0.55f), {0.4f, 0.35f, 0.2f});
    auto rockSolidB = makeSphere(0.7f) - translated(makeSphere(0.5f), {-0.3f, 0.3f, -0.25f});
    Mesh rockMeshA = rockSolidA->buildMesh(16, {1.f, 1.f, 1.f});
    Mesh rockMeshB = rockSolidB->buildMesh(16, {1.f, 1.f, 1.f});
    fuhgl::Texture rockTex = procedural::makeTexture(procedural::voronoi(128, 128, 8, 99u,
                                                                  {0.16f, 0.15f, 0.14f}, {0.55f, 0.53f, 0.50f}));
    Material rockMat = materials::textured(rockTex, {1.f, 1.f, 1.f});

    // --- monolith: cube with a spherical bite, dark stone ------------------------------------
    auto monoSolid = makeCube({0.7f, 2.6f, 0.7f}) - makeSphere(0.55f);
    Mesh monoMesh = monoSolid->buildMesh(28, {1.f, 1.f, 1.f});
    fuhgl::Texture stoneTex = procedural::makeTexture(procedural::voronoi(128, 128, 5, 5u,
                                                                   {0.10f, 0.10f, 0.12f}, {0.38f, 0.36f, 0.34f}));
    Material stoneMat = materials::textured(stoneTex, {1.f, 1.f, 1.f});

    // --- pond: metallic mirror-smooth water slab (SSR sky reflection) ------------------------
    Mesh waterMesh;
    shapes::box(waterMesh, {3.4f, 0.06f, 2.6f}, {1.f, 1.f, 1.f});
    Material waterMat = materials::pbr({0.06f, 0.13f, 0.19f}, 1.0f, 0.06f);

    // --- grass: two thin blade meshes, thousands of F24 instances -----------------------------
    auto makeBlade = [](float w, float h) {
        MeshData d = shapes::boxData({w, h, w}, {1.f, 1.f, 1.f});
        for (size_t v = 0; v < d.vertexCount(); ++v) {
            float* p = d.vertices.data() + v * MeshData::kFloatsPerVertex;
            p[1] += h; // base at y = 0
            const float t = fuhgl::math::clamp(p[1] / (2.f * h), 0.f, 1.f); // dark base -> light tip
            p[6] = 0.10f + 0.25f * t;
            p[7] = 0.24f + 0.31f * t;
            p[8] = 0.08f + 0.12f * t;
        }
        Mesh m;
        m.setMeshData(d);
        return m;
    };
    Mesh grassMeshA = makeBlade(0.014f, 0.32f);
    Mesh grassMeshB = makeBlade(0.022f, 0.5f);
    Material grassBladeMat = materials::matte({1.f, 1.f, 1.f}); // vertex colors carry the gradient
    std::vector<fuhgl::math::Mat4> grassModelsA, grassModelsB;
    grassModelsA.reserve(2000);
    grassModelsB.reserve(1000);
    for (int i = 0; i < 3000; ++i) {
        const float x = (hash01(i, 8, 31) - 0.5f) * 46.f;
        const float z = (hash01(i, 9, 32) - 0.5f) * 46.f - 5.f;
        if (std::sqrt(x * x + (z + 2.f) * (z + 2.f)) < 2.2f) continue;          // keep the clearing walkable
        if (std::sqrt((x - 7.f) * (x - 7.f) + (z + 9.f) * (z + 9.f)) < 3.5f) continue; // keep out of the pond
        const float y = terrainH(x, z);
        const fuhgl::math::Mat4 m = fuhgl::math::translate({x, y, z}) *
                                    fuhgl::math::rotateY(hash01(i, 10, 33) * 6.2831853f) *
                                    fuhgl::math::rotateX((hash01(i, 11, 34) - 0.5f) * 0.35f) *
                                    fuhgl::math::scale({1.f, 0.7f + 0.6f * hash01(i, 12, 35), 1.f});
        if (i < 2000) grassModelsA.push_back(m);
        else grassModelsB.push_back(m);
    }
    grassMeshA.setInstanceData(grassModelsA.data(), static_cast<fuhgl::gl::GLsizei>(grassModelsA.size()));
    grassMeshB.setInstanceData(grassModelsB.data(), static_cast<fuhgl::gl::GLsizei>(grassModelsB.size()));

    // --- scene -----------------------------------------------------------------------------
    SceneView scene;
    scene.camera = &cam;
    scene.width = args.width;
    scene.height = args.height;
    scene.aspect = static_cast<float>(args.width) / static_cast<float>(args.height);
    scene.clearColor = night ? fuhgl::math::Vec3{0.01f, 0.015f, 0.04f}
                     : sunset ? fuhgl::math::Vec3{0.10f, 0.05f, 0.10f}
                              : fuhgl::math::Vec3{0.05f, 0.07f, 0.12f};
    scene.ambient = night ? fuhgl::math::Vec3{0.04f, 0.05f, 0.09f}
                  : sunset ? fuhgl::math::Vec3{0.20f, 0.12f, 0.10f}
                           : fuhgl::math::Vec3{0.16f, 0.17f, 0.20f};
    scene.fogColor = night ? fuhgl::math::Vec3{0.02f, 0.03f, 0.07f}
                   : sunset ? fuhgl::math::Vec3{0.55f, 0.25f, 0.20f}
                            : fuhgl::math::Vec3{0.65f, 0.75f, 0.90f};
    scene.fogDensity = night ? 0.004f : sunset ? 0.005f : 0.003f;
    scene.items.push_back({&terrainMesh, &terrainMat, fuhgl::math::Mat4{}});
    scene.items.push_back({&waterMesh, &waterMat, fuhgl::math::translate({7.f, -1.02f, -9.f})});
    const fuhgl::math::Vec3 monoPos{-4.5f, terrainH(-4.5f, -6.f) + 2.4f, -6.f};
    scene.items.push_back({&monoMesh, &stoneMat, fuhgl::math::translate(monoPos) * fuhgl::math::rotateY(0.5f)});

    // trees + bushes + rocks scattered over the hills (deterministic jitter, avoid clearing/pond)
    for (int i = 0; i < 40; ++i) {
        const float x = (hash01(i, 3, 21) - 0.5f) * 52.f;
        const float z = (hash01(i, 4, 22) - 0.5f) * 52.f - 6.f;
        if (std::abs(x) > 26.f || std::abs(z) > 26.f) continue;
        if (std::sqrt(x * x + (z + 2.f) * (z + 2.f)) < 3.5f) continue;
        if (std::sqrt((x - 7.f) * (x - 7.f) + (z + 9.f) * (z + 9.f)) < 4.5f) continue;
        const float y = terrainH(x, z);
        const float s = 0.7f + hash01(i, 5, 23) * 0.9f;
        const fuhgl::math::Mat4 t = fuhgl::math::translate({x, y - 0.05f, z}) *
                                    fuhgl::math::rotateY(hash01(i, 6, 24) * 6.2831853f) *
                                    fuhgl::math::scale({s, s, s});
        if (hash01(i, 7, 25) > 0.6f) {
            scene.items.push_back({&broadTrunk, &trunkMat, t});
            scene.items.push_back({&broadCanopy, &broadMat, t});
        } else {
            scene.items.push_back({&pineTrunk, &trunkMat, t});
            scene.items.push_back({&pineCanopy, &pineMat, t});
        }
    }
    for (int i = 0; i < 22; ++i) {
        const float x = (hash01(i, 13, 41) - 0.5f) * 44.f;
        const float z = (hash01(i, 14, 42) - 0.5f) * 44.f - 5.f;
        if (std::sqrt(x * x + (z + 2.f) * (z + 2.f)) < 2.6f) continue;
        if (std::sqrt((x - 7.f) * (x - 7.f) + (z + 9.f) * (z + 9.f)) < 3.8f) continue;
        const float s = 0.5f + hash01(i, 15, 43) * 0.9f;
        Mesh& bm = (i % 2 == 0) ? bushMeshA : bushMeshB;
        scene.items.push_back({&bm, &bushMat,
                               fuhgl::math::translate({x, terrainH(x, z) - 0.08f, z}) *
                               fuhgl::math::rotateY(hash01(i, 16, 44) * 6.2831853f) *
                               fuhgl::math::scale({s, s * 0.75f, s})});
    }
    for (int i = 0; i < 14; ++i) {
        const float x = (hash01(i, 1, 15) - 0.5f) * 48.f;
        const float z = (hash01(i, 2, 16) - 0.5f) * 48.f - 5.f;
        if (std::sqrt(x * x + (z + 2.f) * (z + 2.f)) < 2.4f) continue;
        if (std::sqrt((x - 7.f) * (x - 7.f) + (z + 9.f) * (z + 9.f)) < 3.6f) continue;
        const float s = 0.4f + hash01(i, 17, 45) * 1.4f;
        Mesh& rm = (i % 2 == 0) ? rockMeshA : rockMeshB;
        scene.items.push_back({&rm, &rockMat,
                               fuhgl::math::translate({x, terrainH(x, z) + 0.05f * s, z}) *
                               fuhgl::math::rotateY(hash01(i, 18, 46) * 6.2831853f) *
                               fuhgl::math::scale({s, s * 0.8f, s})});
    }
    scene.items.push_back({&grassMeshA, &grassBladeMat, fuhgl::math::Mat4{}});
    scene.items.push_back({&grassMeshB, &grassBladeMat, fuhgl::math::Mat4{}});

    // sun: day = high warm light; sunset = low in front of the camera (god rays in frame);
    // night = dim blue "moon"
    const Light sun = night ? Light::directional({-0.3f, -1.f, -0.2f}, {0.35f, 0.45f, 0.70f}, 0.9f)
                    : sunset ? Light::directional({0.25f, -0.10f, 1.f}, {1.f, 0.45f, 0.20f}, 3.0f)
                             : Light::directional({-0.4f, -1.f, -0.25f}, {1.f, 0.96f, 0.88f}, 2.6f);
    scene.lights.push_back(sun);
    scene.lights.push_back(Light::point({-6.f, 5.f, 4.f},
                                        night ? fuhgl::math::Vec3{0.9f, 0.55f, 0.25f}
                                        : sunset ? fuhgl::math::Vec3{0.5f, 0.25f, 0.15f}
                                                 : fuhgl::math::Vec3{0.45f, 0.55f, 0.85f}, 10.f, 18.f));

    // night: IBL environment baked from a dark gradient cubemap + emissive lamp posts
    // whose reflections show up in the ground via SSR
    fuhgl::render::Environment env;
    Mesh lampMesh;           // must outlive the scene: items store &lampMesh
    Material lampMat;
    if (night) {
        fuhgl::Skybox nightSky;
        nightSky.create();
        nightSky.makeGradient({0.01f, 0.02f, 0.07f}, {0.10f, 0.12f, 0.22f}, 64);
        if (!env.createFromCubemap(nightSky.texture())) {
            std::fprintf(stderr, "showcase: night IBL bake failed\n");
            return 1;
        }
        env.intensity = 1.0f;
        scene.env = &env;
        shapes::box(lampMesh, {0.12f, 0.9f, 0.12f}, {1.f, 1.f, 1.f});
        lampMat = materials::emissive({1.f, 0.72f, 0.35f}, 6.f);
        const fuhgl::math::Vec3 lampPos[3] = {{-3.f, 0.f, 2.f}, {3.5f, 0.f, -3.f}, {0.5f, 0.f, -7.f}};
        for (const auto& p : lampPos) {
            const float ly = terrainH(p.x, p.z);
            scene.items.push_back({&lampMesh, &lampMat, fuhgl::math::translate({p.x, ly + 0.9f, p.z})});
            scene.lights.push_back(Light::point({p.x, ly + 1.8f, p.z}, {1.f, 0.7f, 0.4f}, 14.f, 16.f));
        }
    }

    TextRenderer text;
    if (!text.create()) {
        std::fprintf(stderr, "showcase: TextRenderer create failed\n");
        return 1;
    }
    auto drawHud = [&](float fpsValue) {
        char line[200];
        text.begin(scene.width, scene.height);
        text.draw(night ? "FuhGLCore showcase - night (IBL + SSR)"
                : sunset ? "FuhGLCore showcase - sunset (god rays)"
                         : "FuhGLCore showcase - day",
                  14.f, 12.f, 2.f, {1.f, 1.f, 1.f, 1.f});
        std::snprintf(line, sizeof(line),
                      "deferred renderer: CSM + SSAO + HDR + bloom + god rays + TAA + SSR + vol fog + DoF | FPS: %.1f",
                      fpsValue);
        text.draw(line, 14.f, 40.f, 1.f, {0.85f, 0.88f, 0.95f, 1.f});
        text.end();
    };

    if (headless) {
        for (int i = 0; i < 8; ++i) renderer.render(scene); // let TAA history accumulate
        fuhgl::gl::flush();
        renderer.output().bind();
        drawHud(60.0f);
        fuhgl::gl::flush();
        std::vector<unsigned char> px(static_cast<size_t>(args.width) * args.height * 4);
        fuhgl::gl::readPixels(0, 0, args.width, args.height, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        std::vector<unsigned char> flipped(px.size());
        const size_t stride = static_cast<size_t>(args.width) * 4;
        for (int y = 0; y < args.height; ++y)
            std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                        px.data() + static_cast<size_t>(args.height - 1 - y) * stride, stride);
        if (!screenshot::writePng(args.screenshot.c_str(), flipped.data(), args.width, args.height)) {
            std::fprintf(stderr, "showcase: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "showcase: wrote %s (mode=%s, %dx%d, GL %d.%d)\n", args.screenshot.c_str(),
                     args.mode.c_str(), args.width, args.height,
                     fuhgl::gl::version().major, fuhgl::gl::version().minor);
        return 0;
    }

    cam.attach(window);
    window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
        if (action == fuhgl::Action::Press && key == fuhgl::Key::Escape) window.setShouldClose(true);
        cam.onKey(key, action);
    });
    FpsCounter fps;
    double lastTime = window.time();
    while (!window.shouldClose()) {
        window.pollEvents();
        const double now = window.time();
        fps.frame(now);
        cam.update(static_cast<float>(now - lastTime));
        lastTime = now;
        scene.width = window.framebufferWidth();
        scene.height = window.framebufferHeight();
        scene.aspect = static_cast<float>(scene.width) / static_cast<float>(scene.height);
        if (scene.width != renderer.width() || scene.height != renderer.height())
            renderer.resize(scene.width, scene.height);
        renderer.render(scene);
        renderer.output().bind();
        drawHud(fps.fps());
        fuhgl::Framebuffer::blitToDefault(renderer.output(), scene.width, scene.height);
        window.swapBuffers();
    }
    return 0;
}
