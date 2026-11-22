#include "fuhgl/render/terrain.hpp"

#include <algorithm>
#include <cmath>

namespace fuhgl::render {

float heightmapSample(const float* heights, int w, int h, float u, float v) {
    if (!heights || w < 2 || h < 2) return 0.f;
    u = math::clamp(u, 0.f, 1.f);
    v = math::clamp(v, 0.f, 1.f);
    const float x = u * static_cast<float>(w - 1);
    const float y = v * static_cast<float>(h - 1);
    const int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
    const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
    const float h00 = heights[static_cast<size_t>(y0) * w + x0];
    const float h10 = heights[static_cast<size_t>(y0) * w + x1];
    const float h01 = heights[static_cast<size_t>(y1) * w + x0];
    const float h11 = heights[static_cast<size_t>(y1) * w + x1];
    return (h00 * (1.f - fx) + h10 * fx) * (1.f - fy) + (h01 * (1.f - fx) + h11 * fx) * fy;
}

float heightmapSample(const procedural::Image& img, float u, float v) {
    if (img.width < 2 || img.height < 2 || img.rgba.empty()) return 0.f;
    // sample the red channel as a 0..255 height directly (no float copy)
    u = math::clamp(u, 0.f, 1.f);
    v = math::clamp(v, 0.f, 1.f);
    const float x = u * static_cast<float>(img.width - 1);
    const float y = v * static_cast<float>(img.height - 1);
    const int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, img.width - 1), y1 = std::min(y0 + 1, img.height - 1);
    const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
    const float h00 = img.at(x0, y0)[0], h10 = img.at(x1, y0)[0];
    const float h01 = img.at(x0, y1)[0], h11 = img.at(x1, y1)[0];
    return ((h00 * (1.f - fx) + h10 * fx) * (1.f - fy) + (h01 * (1.f - fx) + h11 * fx) * fy) / 255.f;
}

MeshData heightmapTerrainData(const float* heights, int w, int h,
                              float sizeX, float sizeZ, float heightScale,
                              float uvScale,
                              const math::Vec3& lowColor,
                              const math::Vec3& highColor,
                              const math::Vec3& steepColor) {
    MeshData md;
    if (!heights || w < 2 || h < 2) return md;
    const float stepX = sizeX / static_cast<float>(w - 1);
    const float stepZ = sizeZ / static_cast<float>(h - 1);

    // height range for color normalization
    float hMin = heights[0], hMax = heights[0];
    for (int i = 0; i < w * h; ++i) {
        hMin = std::min(hMin, heights[i]);
        hMax = std::max(hMax, heights[i]);
    }
    const float hRange = std::max(hMax - hMin, 1e-5f);

    auto hAt = [&](int x, int z) {
        x = std::clamp(x, 0, w - 1);
        z = std::clamp(z, 0, h - 1);
        return heights[static_cast<size_t>(z) * w + x];
    };

    md.vertices.reserve(static_cast<size_t>(w) * h * MeshData::kFloatsPerVertex);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) {
            const float x = -sizeX * 0.5f + i * stepX;
            const float z = -sizeZ * 0.5f + j * stepZ;
            const float hv = hAt(i, j);
            const float y = hv * heightScale;
            // central-difference normal (clamped at borders)
            const float dx = (hAt(i + 1, j) - hAt(i - 1, j)) * heightScale;
            const float dz = (hAt(i, j + 1) - hAt(i, j - 1)) * heightScale;
            const math::Vec3 n = math::normalize({-dx / stepX, 2.f, -dz / stepZ});
            // height-based color, shifted to rock on steep slopes
            const float t = (hv - hMin) / hRange;
            math::Vec3 c{lowColor.x + (highColor.x - lowColor.x) * t,
                         lowColor.y + (highColor.y - lowColor.y) * t,
                         lowColor.z + (highColor.z - lowColor.z) * t};
            const float steep = math::clamp((0.78f - n.y) / 0.28f, 0.f, 1.f); // n.y < 0.78 -> rock
            c = {c.x + (steepColor.x - c.x) * steep,
                 c.y + (steepColor.y - c.y) * steep,
                 c.z + (steepColor.z - c.z) * steep};
            const float v[] = {x, y, z, n.x, n.y, n.z, c.x, c.y, c.z, x * uvScale, z * uvScale};
            md.vertices.insert(md.vertices.end(), v, v + MeshData::kFloatsPerVertex);
        }
    md.indices.reserve(static_cast<size_t>(w - 1) * (h - 1) * 6);
    for (int j = 0; j < h - 1; ++j)
        for (int i = 0; i < w - 1; ++i) {
            const unsigned v00 = static_cast<unsigned>(j * w + i);
            const unsigned v10 = v00 + 1, v01 = v00 + w, v11 = v01 + 1;
            const unsigned tri[] = {v00, v01, v11, v00, v11, v10}; // CCW seen from +Y
            md.indices.insert(md.indices.end(), tri, tri + 6);
        }
    return md;
}

struct HeightmapTerrain::Impl {};

HeightmapTerrain::HeightmapTerrain() : impl_(std::make_unique<Impl>()) {}
HeightmapTerrain::~HeightmapTerrain() = default;
HeightmapTerrain::HeightmapTerrain(HeightmapTerrain&&) noexcept = default;
HeightmapTerrain& HeightmapTerrain::operator=(HeightmapTerrain&&) noexcept = default;

bool HeightmapTerrain::create(const float* heights, int w, int h, float sizeX, float sizeZ, float heightScale) {
    if (!heights || w < 2 || h < 2) return false;
    heights_.assign(heights, heights + static_cast<size_t>(w) * h);
    w_ = w;
    h_ = h;
    sizeX_ = sizeX;
    sizeZ_ = sizeZ;
    heightScale_ = heightScale;
    MeshData data = heightmapTerrainData(heights_.data(), w_, h_, sizeX_, sizeZ_, heightScale_);
    if (!mesh_.create()) return false;
    mesh_.setMeshData(data);
    return true;
}

bool HeightmapTerrain::create(const procedural::Image& img, float sizeX, float sizeZ, float heightScale) {
    if (img.width < 2 || img.height < 2 || img.rgba.empty()) return false;
    std::vector<float> hs(static_cast<size_t>(img.width) * img.height);
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x)
            hs[static_cast<size_t>(y) * img.width + x] = img.at(x, y)[0] / 255.f;
    return create(hs.data(), img.width, img.height, sizeX, sizeZ, heightScale);
}

float HeightmapTerrain::heightAt(float x, float z) const {
    if (w_ < 2 || h_ < 2 || sizeX_ <= 0.f || sizeZ_ <= 0.f) return 0.f;
    const float u = math::clamp((x + sizeX_ * 0.5f) / sizeX_, 0.f, 1.f);
    const float v = math::clamp((z + sizeZ_ * 0.5f) / sizeZ_, 0.f, 1.f);
    return heightmapSample(heights_.data(), w_, h_, u, v) * heightScale_;
}

} // namespace fuhgl::render
