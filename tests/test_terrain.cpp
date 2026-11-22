// Unit tests for F38 heightmap terrain: heightmapSample (bilinear/clamp), procedural
// heightmap image (deterministic, range), heightmapTerrainData (counts, normals, winding,
// colors, uv) and the HeightmapTerrain height mapping (no GL needed for the math parts).

#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/math.hpp>
#include <fuhgl/render/procedural.hpp>
#include <fuhgl/render/terrain.hpp>

#include <vector>

using namespace fuhgl;

namespace {

// flat 4x4 heightmap, all 0.5
std::vector<float> flatMap(int w, int h, float v) { return std::vector<float>(size_t(w) * h, v); }

} // namespace

void runTerrainTests() {
    // --- heightmapSample -------------------------------------------------------
    {
        auto m = flatMap(4, 4, 0.5f);
        CHECK_NEAR(render::heightmapSample(m.data(), 4, 4, 0.f, 0.f), 0.5f, 1e-6f);
        CHECK_NEAR(render::heightmapSample(m.data(), 4, 4, 1.f, 1.f), 0.5f, 1e-6f);
        CHECK_NEAR(render::heightmapSample(m.data(), 4, 4, 0.5f, 0.5f), 0.5f, 1e-6f);
        // clamping outside [0,1]
        CHECK_NEAR(render::heightmapSample(m.data(), 4, 4, -1.f, 2.f), 0.5f, 1e-6f);
        // bilinear on a ramp: heights[i] = i/(w-1) along x
        std::vector<float> ramp(4 * 4);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) ramp[size_t(y) * 4 + x] = float(x) / 3.f;
        CHECK_NEAR(render::heightmapSample(ramp.data(), 4, 4, 0.f, 0.f), 0.f, 1e-6f);
        CHECK_NEAR(render::heightmapSample(ramp.data(), 4, 4, 1.f, 0.7f), 1.f, 1e-6f);
        CHECK_NEAR(render::heightmapSample(ramp.data(), 4, 4, 0.5f, 0.f), 0.5f, 1e-6f); // x = 1.5 -> 0.5
        CHECK_NEAR(render::heightmapSample(ramp.data(), 4, 4, 1.f / 6.f, 0.f), 1.f / 6.f, 1e-6f);
        // degenerate input
        CHECK(render::heightmapSample(nullptr, 4, 4, 0.5f, 0.5f) == 0.f);
    }
    // --- procedural::heightmap image --------------------------------------------
    {
        auto a = render::procedural::heightmap(64, 48, 42u);
        auto b = render::procedural::heightmap(64, 48, 42u);
        auto c = render::procedural::heightmap(64, 48, 43u);
        CHECK(a.width == 64 && a.height == 48);
        CHECK(a.rgba.size() == 64u * 48u * 4u);
        CHECK(a.rgba == b.rgba); // deterministic
        CHECK(a.rgba != c.rgba); // seed matters
        // grayscale + alpha
        bool gray = true, alpha = true;
        float mn = 1.f, mx = 0.f;
        for (size_t i = 0; i < a.rgba.size(); i += 4) {
            if (a.rgba[i] != a.rgba[i + 1] || a.rgba[i] != a.rgba[i + 2]) gray = false;
            if (a.rgba[i + 3] != 255) alpha = false;
            mn = std::min(mn, a.rgba[i] / 255.f);
            mx = std::max(mx, a.rgba[i] / 255.f);
        }
        CHECK(gray && alpha);
        CHECK(mx > 0.5f && mn < 0.5f); // real dynamic range
        // image sampler matches float sampler of the same values
        std::vector<float> hs(64u * 48u);
        for (size_t i = 0; i < hs.size(); ++i) hs[i] = a.rgba[i * 4] / 255.f;
        CHECK_NEAR(render::heightmapSample(a, 0.37f, 0.61f),
                   render::heightmapSample(hs.data(), 64, 48, 0.37f, 0.61f), 1.f / 255.f);
    }
    // --- heightmapTerrainData ----------------------------------------------------
    {
        const int W = 8, H = 6;
        auto m = flatMap(W, H, 0.25f);
        render::MeshData md = render::heightmapTerrainData(m.data(), W, H, 10.f, 8.f, 4.f);
        CHECK(md.vertexCount() == size_t(W) * H);
        CHECK(md.indices.size() == size_t(W - 1) * (H - 1) * 6);
        // flat map -> all normals straight up, y = 0.25 * 4 = 1
        bool up = true;
        for (size_t v = 0; v < md.vertexCount(); ++v) {
            const float* p = md.vertices.data() + v * render::MeshData::kFloatsPerVertex;
            if (std::abs(p[3]) > 1e-5f || std::abs(p[4] - 1.f) > 1e-5f || std::abs(p[5]) > 1e-5f) up = false;
            if (std::abs(p[1] - 1.f) > 1e-5f) up = false;
        }
        CHECK(up);
        // corners at +/- size/2
        const float* p0 = md.vertices.data();
        CHECK_NEAR(p0[0], -5.f, 1e-5f);
        CHECK_NEAR(p0[2], -4.f, 1e-5f);
        const float* pLast = md.vertices.data() + (md.vertexCount() - 1) * render::MeshData::kFloatsPerVertex;
        CHECK_NEAR(pLast[0], 5.f, 1e-5f);
        CHECK_NEAR(pLast[2], 4.f, 1e-5f);
        // uv scaled by uvScale (default 0.1): uv of corner = (-0.5, -0.4)
        CHECK_NEAR(p0[9], -0.5f, 1e-5f);
        CHECK_NEAR(p0[10], -0.4f, 1e-5f);
        // winding: first triangle normal via cross product must be +Y
        const unsigned i0 = md.indices[0], i1 = md.indices[1], i2 = md.indices[2];
        const float* a = md.vertices.data() + i0 * render::MeshData::kFloatsPerVertex;
        const float* b = md.vertices.data() + i1 * render::MeshData::kFloatsPerVertex;
        const float* c = md.vertices.data() + i2 * render::MeshData::kFloatsPerVertex;
        math::Vec3 e1{b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        math::Vec3 n = math::cross(e1, e2);
        CHECK(n.y > 0.f);
        // height coloring: single-peak map -> center brighter (highColor) than border (lowColor)
        std::vector<float> peak(W * H, 0.f);
        peak[size_t((H / 2) * W + W / 2)] = 1.f;
        render::MeshData pd = render::heightmapTerrainData(peak.data(), W, H, 10.f, 8.f, 4.f,
                                                           0.1f, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f, 0.f});
        const float* pc = pd.vertices.data() + (size_t(H / 2) * W + W / 2) * render::MeshData::kFloatsPerVertex;
        const float* pb = pd.vertices.data();
        CHECK(pc[6] > pb[6] && pc[7] > pb[7] && pc[8] > pb[8]); // center = highColor (white)
        // degenerate input -> empty
        CHECK(render::heightmapTerrainData(nullptr, 4, 4, 1.f, 1.f, 1.f).vertices.empty());
    }
    // --- HeightmapTerrain mapping (CPU part: heightAt via a created-from-float copy is
    // GL-bound; here verify the sampling math against heightAt's formula) -----------
    {
        // 4x4 ramp along x: heightAt(x, z) with sizeX=6 -> u=(x+3)/6
        std::vector<float> ramp(4 * 4);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) ramp[size_t(y) * 4 + x] = float(x) / 3.f;
        const float scale = 6.f;
        auto expectAt = [&](float x, float z) {
            const float u = math::clamp((x + 3.f) / 6.f, 0.f, 1.f);
            const float v = math::clamp((z + 3.f) / 6.f, 0.f, 1.f);
            return render::heightmapSample(ramp.data(), 4, 4, u, v) * scale;
        };
        CHECK_NEAR(expectAt(-3.f, 0.f), 0.f, 1e-5f);
        CHECK_NEAR(expectAt(3.f, 0.f), 6.f, 1e-5f);
        CHECK_NEAR(expectAt(0.f, 0.f), 3.f, 1e-5f); // u=0.5 -> ramp 0.5 -> 3
    }
}
