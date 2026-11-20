#include "fuhgl/render/mesh.hpp"
#include "fuhgl/gl/loader.hpp"
#include "fuhgl/gl/state.hpp"

namespace fuhgl::render {

struct Mesh::Impl {};

Mesh::Mesh() : impl_(std::make_unique<Impl>()) {}
Mesh::~Mesh() = default;
Mesh::Mesh(Mesh&&) noexcept = default;
Mesh& Mesh::operator=(Mesh&&) noexcept = default;

bool Mesh::create() {
    vertexCount_ = 0;
    indexCount_ = 0;
    return vao_.create();
}

void Mesh::setData(const void* data, gl::GLsizeiptr bytes, gl::GLsizei vertexCount,
                   const std::vector<VertexAttrib>& attribs, Buffer::Usage usage) {
    vbo_.bind();
    vbo_.setData(bytes, data, usage);
    vao_.bind();
    for (const VertexAttrib& a : attribs)
        vao_.setAttrib(a.index, a.size, a.type, a.normalized, a.stride, a.offset);
    vertexCount_ = vertexCount;
}

void Mesh::setIndices(const void* data, gl::GLsizei indexCount, gl::GLenum indexType, Buffer::Usage usage) {
    ibo_.bind();
    ibo_.setData(indexCount * (indexType == gl::GL_UNSIGNED_SHORT ? 2 : 4), data, usage);
    vao_.bind();
    vao_.setElementBuffer(ibo_);
    indexCount_ = indexCount;
    indexType_ = indexType;
}

void Mesh::draw() const {
    vao_.bind();
    if (indexCount_ > 0)
        gl::drawElements(gl::Primitive::Triangles, indexCount_, indexType_, nullptr);
    else
        gl::drawArrays(gl::Primitive::Triangles, 0, vertexCount_);
}

namespace shapes {

void box(Mesh& mesh, const math::Vec3& h, const math::Vec3& color) {
    // 6 faces, 4 verts each: pos(3) normal(3) color(3). Axis vectors have one +/-1
    // component, so component-wise multiply with h yields the right extent per axis.
    struct V { float p[3]; float n[3]; float c[3]; };
    const math::Vec3 n[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const math::Vec3 u[6] = {{0,0,1},{0,0,1},{1,0,0},{1,0,0},{1,0,0},{1,0,0}};
    const math::Vec3 v[6] = {{0,1,0},{0,1,0},{0,0,1},{0,0,1},{0,1,0},{0,1,0}};
    std::vector<V> verts;
    std::vector<unsigned int> idx;
    for (int f = 0; f < 6; ++f) {
        const unsigned int base = static_cast<unsigned int>(verts.size());
        for (int i = 0; i < 4; ++i) {
            const float su = (i == 1 || i == 2) ? 1.f : -1.f;
            const float sv = (i >= 2) ? 1.f : -1.f;
            const math::Vec3 p = n[f] * h + (u[f] * h) * su + (v[f] * h) * sv;
            V vv{};
            vv.p[0] = p.x; vv.p[1] = p.y; vv.p[2] = p.z;
            vv.n[0] = n[f].x; vv.n[1] = n[f].y; vv.n[2] = n[f].z;
            vv.c[0] = color.x; vv.c[1] = color.y; vv.c[2] = color.z;
            verts.push_back(vv);
        }
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    mesh.create();
    mesh.setData(verts.data(), static_cast<gl::GLsizeiptr>(verts.size() * sizeof(V)),
                 static_cast<gl::GLsizei>(verts.size()),
                 {{0, 3, gl::GL_FLOAT, false, sizeof(V), offsetof(V, p)},
                  {1, 3, gl::GL_FLOAT, false, sizeof(V), offsetof(V, n)},
                  {2, 3, gl::GL_FLOAT, false, sizeof(V), offsetof(V, c)}});
    mesh.setIndices(idx.data(), static_cast<gl::GLsizei>(idx.size()), gl::GL_UNSIGNED_INT);
}

void quad(Mesh& mesh, float halfSize, const math::Vec3& color) {
    struct V { float p[3]; float n[3]; float c[3]; };
    const V verts[4] = {
        {{-halfSize, -halfSize, 0}, {0, 0, 1}, {color.x, color.y, color.z}},
        {{ halfSize, -halfSize, 0}, {0, 0, 1}, {color.x, color.y, color.z}},
        {{ halfSize,  halfSize, 0}, {0, 0, 1}, {color.x, color.y, color.z}},
        {{-halfSize,  halfSize, 0}, {0, 0, 1}, {color.x, color.y, color.z}},
    };
    const unsigned int idx[6] = {0, 1, 2, 0, 2, 3};
    mesh.create();
    mesh.setData(verts, sizeof(verts), 4,
                 {{0, 3, gl::GL_FLOAT, false, sizeof(V), offsetof(V, p)},
                  {1, 3, gl::GL_FLOAT, false, sizeof(V), offsetof(V, n)},
                  {2, 3, gl::GL_FLOAT, false, sizeof(V), offsetof(V, c)}});
    mesh.setIndices(idx, 6, gl::GL_UNSIGNED_INT);
}

} // namespace shapes

} // namespace fuhgl::render
