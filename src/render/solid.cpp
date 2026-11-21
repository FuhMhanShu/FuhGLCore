#include "fuhgl/render/solid.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace fuhgl::render {
namespace {

// --- primitives -------------------------------------------------------------------

class CubeSolid : public Solid {
public:
    explicit CubeSolid(math::Vec3 h) : h_(h) {}
    bool inside(const math::Vec3& p) const override {
        return std::fabs(p.x) <= h_.x && std::fabs(p.y) <= h_.y && std::fabs(p.z) <= h_.z;
    }
    math::Vec3 aabbMin() const override { return -h_; }
    math::Vec3 aabbMax() const override { return h_; }
private:
    math::Vec3 h_;
};

class SphereSolid : public Solid {
public:
    explicit SphereSolid(float r) : r_(r) {}
    bool inside(const math::Vec3& p) const override { return p.x * p.x + p.y * p.y + p.z * p.z <= r_ * r_; }
    math::Vec3 aabbMin() const override { return {-r_, -r_, -r_}; }
    math::Vec3 aabbMax() const override { return {r_, r_, r_}; }
private:
    float r_;
};

class CylinderSolid : public Solid {
public:
    CylinderSolid(float r, float h) : r_(r), h_(h) {}
    bool inside(const math::Vec3& p) const override {
        return p.x * p.x + p.z * p.z <= r_ * r_ && std::fabs(p.y) <= h_ * 0.5f;
    }
    math::Vec3 aabbMin() const override { return {-r_, -h_ * 0.5f, -r_}; }
    math::Vec3 aabbMax() const override { return {r_, h_ * 0.5f, r_}; }
private:
    float r_, h_;
};

class ConeSolid : public Solid {
public:
    ConeSolid(float r, float h) : r_(r), h_(h) {}
    bool inside(const math::Vec3& p) const override {
        const float y = p.y + h_ * 0.5f; // 0 at base .. h at tip
        if (y < 0.f || y > h_) return false;
        const float rr = r_ * (1.f - y / h_);
        return p.x * p.x + p.z * p.z <= rr * rr;
    }
    math::Vec3 aabbMin() const override { return {-r_, -h_ * 0.5f, -r_}; }
    math::Vec3 aabbMax() const override { return {r_, h_ * 0.5f, r_}; }
private:
    float r_, h_;
};

class TorusSolid : public Solid {
public:
    TorusSolid(float R, float r) : R_(R), r_(r) {}
    bool inside(const math::Vec3& p) const override {
        const float q = std::sqrt(p.x * p.x + p.z * p.z) - R_;
        return q * q + p.y * p.y <= r_ * r_;
    }
    math::Vec3 aabbMin() const override { return {-(R_ + r_), -r_, -(R_ + r_)}; }
    math::Vec3 aabbMax() const override { return {R_ + r_, r_, R_ + r_}; }
private:
    float R_, r_;
};

class OctahedronSolid : public Solid {
public:
    OctahedronSolid(math::Vec3 center, math::Vec3 half) : c_(center), h_(half) {}
    bool inside(const math::Vec3& p) const override {
        const math::Vec3 d = p - c_;
        if (h_.x <= 0.f || h_.y <= 0.f || h_.z <= 0.f) return false;
        return std::fabs(d.x) / h_.x + std::fabs(d.y) / h_.y + std::fabs(d.z) / h_.z <= 1.f;
    }
    math::Vec3 aabbMin() const override { return c_ - h_; }
    math::Vec3 aabbMax() const override { return c_ + h_; }
private:
    math::Vec3 c_, h_;
};

class TransformSolid : public Solid {
public:
    TransformSolid(SolidPtr s, math::Vec3 offset) : s_(std::move(s)), offset_(offset) {}
    bool inside(const math::Vec3& p) const override { return s_->inside(p - offset_); }
    math::Vec3 aabbMin() const override { return s_->aabbMin() + offset_; }
    math::Vec3 aabbMax() const override { return s_->aabbMax() + offset_; }
private:
    SolidPtr s_;
    math::Vec3 offset_;
};

// --- CSG nodes ----------------------------------------------------------------------

enum class CsgOp { Union, Intersection, Difference };

class CsgSolid : public Solid {
public:
    CsgSolid(SolidPtr a, SolidPtr b, CsgOp op) : a_(std::move(a)), b_(std::move(b)), op_(op) {}
    bool inside(const math::Vec3& p) const override {
        const bool ia = a_->inside(p), ib = b_->inside(p);
        switch (op_) {
            case CsgOp::Union: return ia || ib;
            case CsgOp::Intersection: return ia && ib;
            case CsgOp::Difference: return ia && !ib;
        }
        return false;
    }
    math::Vec3 aabbMin() const override {
        const math::Vec3 a = a_->aabbMin(), b = b_->aabbMin();
        if (op_ == CsgOp::Difference) return a; // difference is bounded by a
        return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
    }
    math::Vec3 aabbMax() const override {
        const math::Vec3 a = a_->aabbMax(), b = b_->aabbMax();
        if (op_ == CsgOp::Difference) return a;
        return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
    }
private:
    SolidPtr a_, b_;
    CsgOp op_;
};

// --- marching tetrahedra -------------------------------------------------------------

struct Grid {
    int nx = 0, ny = 0, nz = 0;
    float cell = 1.f;
    math::Vec3 origin{};
    std::vector<float> f;     // (nx*ny*nz) field: -1 inside, +1 outside
    std::vector<math::Vec3> grad;
    int idx(int x, int y, int z) const { return x + nx * (y + ny * z); }
    math::Vec3 pos(int x, int y, int z) const {
        return {origin.x + x * cell, origin.y + y * cell, origin.z + z * cell};
    }
};

Grid sampleField(const Solid& solid, int resolution) {
    Grid g;
    const math::Vec3 mn = solid.aabbMin(), mx = solid.aabbMax();
    const math::Vec3 ext = mx - mn;
    const float maxExt = std::max({ext.x, ext.y, ext.z});
    g.cell = maxExt / static_cast<float>(resolution);
    const float pad = g.cell * 0.5f;
    g.origin = mn - math::Vec3{pad, pad, pad};
    g.nx = std::max(2, static_cast<int>(std::ceil(ext.x / g.cell)) + 2);
    g.ny = std::max(2, static_cast<int>(std::ceil(ext.y / g.cell)) + 2);
    g.nz = std::max(2, static_cast<int>(std::ceil(ext.z / g.cell)) + 2);
    const size_t n = static_cast<size_t>(g.nx) * g.ny * g.nz;
    g.f.assign(n, 1.f);
    g.grad.assign(n, math::Vec3{0.f, 0.f, 1.f});
    for (int z = 0; z < g.nz; ++z)
        for (int y = 0; y < g.ny; ++y)
            for (int x = 0; x < g.nx; ++x)
                g.f[g.idx(x, y, z)] = solid.inside(g.pos(x, y, z)) ? -1.f : 1.f;
    // gradient via central differences (outward: f rises from -1 to +1)
    for (int z = 0; z < g.nz; ++z)
        for (int y = 0; y < g.ny; ++y)
            for (int x = 0; x < g.nx; ++x) {
                const int xm = std::max(0, x - 1), xp = std::min(g.nx - 1, x + 1);
                const int ym = std::max(0, y - 1), yp = std::min(g.ny - 1, y + 1);
                const int zm = std::max(0, z - 1), zp = std::min(g.nz - 1, z + 1);
                const float inv = 0.5f / g.cell;
                g.grad[g.idx(x, y, z)] = {
                    (g.f[g.idx(xp, y, z)] - g.f[g.idx(xm, y, z)]) * inv,
                    (g.f[g.idx(x, yp, z)] - g.f[g.idx(x, ym, z)]) * inv,
                    (g.f[g.idx(x, y, zp)] - g.f[g.idx(x, y, zm)]) * inv};
            }
    return g;
}

} // namespace

MeshData Solid::buildMeshData(int resolution, const math::Vec3& color) const {
    MeshData out;
    if (resolution < 2) resolution = 2;
    const Grid g = sampleField(*this, resolution);
    const math::Vec3 mn = aabbMin(), mx = aabbMax();
    const math::Vec3 ext = mx - mn;

    // cube corner offsets (bit0=x, bit1=y, bit2=z)
    static const int corner[8][3] = {{0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}};
    // 6 tetrahedra sharing the 0-7 body diagonal
    static const int tets[6][4] = {{0,1,3,7},{0,1,5,7},{0,4,5,7},{0,2,3,7},{0,2,6,7},{0,4,6,7}};

    std::map<std::pair<int, int>, unsigned int> vertCache; // grid edge -> vertex index
    std::vector<math::Vec3> normalAccum;                     // per-vertex accumulated face normals

    auto emitVertex = [&](int ga, int gb, float fa, float fb) -> unsigned int {
        const auto key = std::minmax(ga, gb);
        if (auto it = vertCache.find(key); it != vertCache.end()) return it->second;
        const float t = fa / (fa - fb); // fa < 0 (inside), fb > 0
        const int xa = ga % g.nx, ya = (ga / g.nx) % g.ny, za = ga / (g.nx * g.ny);
        const int xb = gb % g.nx, yb = (gb / g.nx) % g.ny, zb = gb / (g.nx * g.ny);
        const math::Vec3 pa = g.pos(xa, ya, za), pb = g.pos(xb, yb, zb);
        const math::Vec3 p = pa + (pb - pa) * t;
        const float uv[2] = {ext.x > 0.f ? (p.x - mn.x) / ext.x : 0.f,
                             ext.z > 0.f ? (p.z - mn.z) / ext.z : 0.f};
        const float vert[MeshData::kFloatsPerVertex] = {
            p.x, p.y, p.z, 0.f, 0.f, 0.f, color.x, color.y, color.z, uv[0], uv[1]};
        const unsigned int vi = static_cast<unsigned int>(out.vertexCount());
        out.vertices.insert(out.vertices.end(), vert, vert + MeshData::kFloatsPerVertex);
        normalAccum.push_back({0.f, 0.f, 0.f});
        vertCache.emplace(key, vi);
        return vi;
    };

    auto emitTri = [&](unsigned int a, unsigned int b, unsigned int c,
                       const math::Vec3& gradHint) {
        float* pa = out.vertices.data() + a * MeshData::kFloatsPerVertex;
        float* pb = out.vertices.data() + b * MeshData::kFloatsPerVertex;
        float* pc = out.vertices.data() + c * MeshData::kFloatsPerVertex;
        const math::Vec3 e1{pb[0]-pa[0], pb[1]-pa[1], pb[2]-pa[2]};
        const math::Vec3 e2{pc[0]-pa[0], pc[1]-pa[1], pc[2]-pa[2]};
        math::Vec3 n = math::cross(e1, e2);
        if (math::dot(n, gradHint) < 0.f) { std::swap(b, c); std::swap(pb, pc); n = -n; }
        normalAccum[a] += n;
        normalAccum[b] += n;
        normalAccum[c] += n;
        out.indices.push_back(a);
        out.indices.push_back(b);
        out.indices.push_back(c);
    };

    for (int z = 0; z < g.nz - 1; ++z)
        for (int y = 0; y < g.ny - 1; ++y)
            for (int x = 0; x < g.nx - 1; ++x) {
                const int gc[8] = {g.idx(x+corner[0][0], y+corner[0][1], z+corner[0][2]),
                                   g.idx(x+corner[1][0], y+corner[1][1], z+corner[1][2]),
                                   g.idx(x+corner[2][0], y+corner[2][1], z+corner[2][2]),
                                   g.idx(x+corner[3][0], y+corner[3][1], z+corner[3][2]),
                                   g.idx(x+corner[4][0], y+corner[4][1], z+corner[4][2]),
                                   g.idx(x+corner[5][0], y+corner[5][1], z+corner[5][2]),
                                   g.idx(x+corner[6][0], y+corner[6][1], z+corner[6][2]),
                                   g.idx(x+corner[7][0], y+corner[7][1], z+corner[7][2])};
                const float fc[8] = {g.f[gc[0]], g.f[gc[1]], g.f[gc[2]], g.f[gc[3]],
                                     g.f[gc[4]], g.f[gc[5]], g.f[gc[6]], g.f[gc[7]]};
                for (const auto& tet : tets) {
                    const int v[4] = {tet[0], tet[1], tet[2], tet[3]};
                    const float fv[4] = {fc[v[0]], fc[v[1]], fc[v[2]], fc[v[3]]};
                    const bool in[4] = {fv[0] < 0.f, fv[1] < 0.f, fv[2] < 0.f, fv[3] < 0.f};
                    int insideCount = 0, firstIn = -1, secondIn = -1, firstOut = -1, secondOut = -1;
                    for (int i = 0; i < 4; ++i) {
                        if (in[i]) { ++insideCount; if (firstIn < 0) firstIn = i; else secondIn = i; }
                        else { if (firstOut < 0) firstOut = i; else secondOut = i; }
                    }
                    if (insideCount == 0 || insideCount == 4) continue;
                    const math::Vec3 gradHint = (g.grad[gc[v[0]]] + g.grad[gc[v[1]]] +
                                                 g.grad[gc[v[2]]] + g.grad[gc[v[3]]]) * 0.25f;
                    const auto edgeVert = [&](int a, int b) {
                        return emitVertex(gc[v[a]], gc[v[b]], fv[a], fv[b]);
                    };
                    if (insideCount == 1 || insideCount == 3) {
                        // one vertex on one side: the 3 crossing edges form a triangle
                        const int lone = insideCount == 1 ? firstIn : firstOut;
                        int others[3], no = 0;
                        for (int i = 0; i < 4; ++i) if (i != lone) others[no++] = i;
                        emitTri(edgeVert(lone, others[0]), edgeVert(lone, others[1]),
                                edgeVert(lone, others[2]), gradHint);
                    } else {
                        // 2 inside (firstIn, secondIn), 2 outside (firstOut, secondOut):
                        // quad in cyclic order (i,k),(j,k),(j,l),(i,l) -> 2 triangles
                        const unsigned int q0 = edgeVert(firstIn, firstOut);
                        const unsigned int q1 = edgeVert(secondIn, firstOut);
                        const unsigned int q2 = edgeVert(secondIn, secondOut);
                        const unsigned int q3 = edgeVert(firstIn, secondOut);
                        emitTri(q0, q1, q2, gradHint);
                        emitTri(q0, q2, q3, gradHint);
                    }
                }
            }

    // finalize: per-vertex normals = normalized accumulated face normals
    for (size_t v = 0; v < out.vertexCount(); ++v) {
        math::Vec3 n = math::normalize(normalAccum[v]);
        if (n.x == 0.f && n.y == 0.f && n.z == 0.f) n = {0.f, 0.f, 1.f};
        float* p = out.vertices.data() + v * MeshData::kFloatsPerVertex;
        p[3] = n.x; p[4] = n.y; p[5] = n.z;
    }
    return out;
}

Mesh Solid::buildMesh(int resolution, const math::Vec3& color) const {
    Mesh mesh;
    mesh.setMeshData(buildMeshData(resolution, color));
    return mesh;
}

SolidPtr makeCube(const math::Vec3& halfExtents) { return std::make_shared<CubeSolid>(halfExtents); }
SolidPtr makeSphere(float radius) { return std::make_shared<SphereSolid>(radius); }
SolidPtr makeCylinder(float radius, float height) { return std::make_shared<CylinderSolid>(radius, height); }
SolidPtr makeCone(float radius, float height) { return std::make_shared<ConeSolid>(radius, height); }
SolidPtr makeTorus(float majorRadius, float minorRadius) { return std::make_shared<TorusSolid>(majorRadius, minorRadius); }

SolidPtr translated(const SolidPtr& s, const math::Vec3& offset) { return std::make_shared<TransformSolid>(s, offset); }

SolidPtr makeBoundingBox(const Solid& s) {
    const math::Vec3 mn = s.aabbMin(), mx = s.aabbMax();
    return translated(makeCube((mx - mn) * 0.5f), (mn + mx) * 0.5f);
}

SolidPtr makeBoundingSphere(const Solid& s) {
    const math::Vec3 mn = s.aabbMin(), mx = s.aabbMax();
    const math::Vec3 c = (mn + mx) * 0.5f;
    const float r = math::length(mx - mn) * 0.5f;
    return translated(makeSphere(r), c);
}

SolidPtr makeBoundingOctahedron(const Solid& s) {
    const math::Vec3 mn = s.aabbMin(), mx = s.aabbMax();
    return std::make_shared<OctahedronSolid>((mn + mx) * 0.5f, (mx - mn) * 0.5f);
}

SolidPtr makeBoundingVolume(const Solid& s, BoundingVolume kind) {
    switch (kind) {
        case BoundingVolume::Box: return makeBoundingBox(s);
        case BoundingVolume::Sphere: return makeBoundingSphere(s);
        case BoundingVolume::Octahedron: return makeBoundingOctahedron(s);
    }
    return makeBoundingBox(s);
}

bool intersects(const math::Frustum& frustum, const Solid& s) {
    return frustum.intersectsAabb(s.aabbMin(), s.aabbMax());
}

SolidPtr operator|(const SolidPtr& a, const SolidPtr& b) { return std::make_shared<CsgSolid>(a, b, CsgOp::Union); }
SolidPtr operator&(const SolidPtr& a, const SolidPtr& b) { return std::make_shared<CsgSolid>(a, b, CsgOp::Intersection); }
SolidPtr operator-(const SolidPtr& a, const SolidPtr& b) { return std::make_shared<CsgSolid>(a, b, CsgOp::Difference); }

} // namespace fuhgl::render
