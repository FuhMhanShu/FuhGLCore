#include "fuhgl/render/lod.hpp"

#include <algorithm>
#include <cmath>

namespace fuhgl::render {

bool LodMesh::create(std::vector<Mesh> levels, std::vector<float> distances) {
    if (levels.empty() || distances.size() + 1 != levels.size()) return false;
    for (size_t i = 1; i < distances.size(); ++i)
        if (distances[i] <= distances[i - 1]) return false; // ascending
    bs_ = levels.front().boundingSphere();
    levels_ = std::move(levels);
    distances_ = std::move(distances);
    return true;
}

bool LodMesh::setDistances(std::vector<float> distances) {
    if (distances.size() + 1 != levels_.size()) return false;
    for (size_t i = 1; i < distances.size(); ++i)
        if (distances[i] <= distances[i - 1]) return false;
    distances_ = std::move(distances);
    return true;
}

int LodMesh::selectLevel(float dist) const {
    if (levels_.empty()) return -1;
    int level = 0;
    for (size_t i = 0; i < distances_.size(); ++i)
        if (dist >= distances_[i]) level = static_cast<int>(i) + 1;
    return std::min(level, static_cast<int>(levels_.size()) - 1);
}

const Mesh* LodMesh::select(float dist) const {
    const int l = selectLevel(dist);
    return l < 0 ? nullptr : &levels_[static_cast<size_t>(l)];
}

std::vector<float> LodMesh::defaultDistances(float fovYRadians, float radius,
                                             const std::vector<float>& fractions) {
    std::vector<float> out;
    out.reserve(fractions.size());
    const float t = std::tan(fovYRadians * 0.5f);
    for (float f : fractions) {
        const float fr = f > 1e-6f ? f : 1e-6f;
        out.push_back(radius / (fr * t));
    }
    return out;
}

const Mesh* resolveLodMesh(const DrawItem& item, const math::Vec3& camPos, bool lodEnabled,
                           int* outLevel) {
    if (!item.lod || !lodEnabled) {
        if (outLevel) *outLevel = -1;
        return item.mesh;
    }
    // world-space center of the item = model * bounding-sphere center (of LOD level 0)
    const math::Vec4 bs = item.lod->boundingSphere();
    const math::Vec4 c = item.model * math::Vec4(bs.x, bs.y, bs.z, 1.f);
    const float dist = math::length(math::Vec3(c.x, c.y, c.z) - camPos);
    const int level = item.lod->selectLevel(dist);
    if (outLevel) *outLevel = level;
    if (level < 0) return item.mesh;
    return &item.lod->level(level);
}

} // namespace fuhgl::render
