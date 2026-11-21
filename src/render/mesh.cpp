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
    instanceCount_ = 0;
    const bool ok = vao_.create();
    vao_.bind();
    for (gl::GLuint i = 4; i < 8; ++i) vao_.disableAttrib(i); // no instance attribute by default
    return ok;
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

void Mesh::setMeshData(const MeshData& data, Buffer::Usage usage) {
    create();
    setData(data.vertices.data(), static_cast<gl::GLsizeiptr>(data.vertices.size() * sizeof(float)),
            static_cast<gl::GLsizei>(data.vertexCount()),
            {{0, 3, gl::GL_FLOAT, false, MeshData::kVertexBytes, 0},
             {1, 3, gl::GL_FLOAT, false, MeshData::kVertexBytes, 3 * sizeof(float)},
             {2, 3, gl::GL_FLOAT, false, MeshData::kVertexBytes, 6 * sizeof(float)},
             {3, 2, gl::GL_FLOAT, false, MeshData::kVertexBytes, 9 * sizeof(float)}},
            usage);
    if (!data.indices.empty())
        setIndices(data.indices.data(), static_cast<gl::GLsizei>(data.indices.size()), gl::GL_UNSIGNED_INT, usage);
}

void Mesh::setInstanceData(const math::Mat4* models, gl::GLsizei count, Buffer::Usage usage) {
    constexpr gl::GLsizei kMat4Bytes = 16 * sizeof(float);
    instanceVbo_.bind();
    instanceVbo_.setData(static_cast<gl::GLsizeiptr>(count) * kMat4Bytes, models, usage);
    vao_.bind();
    // mat4 attribute = 4 vec4 columns at locations 4..7, one per 16 bytes, divisor 1
    for (gl::GLuint i = 0; i < 4; ++i) {
        vao_.setAttrib(4 + i, 4, gl::GL_FLOAT, false, kMat4Bytes, static_cast<gl::GLintptr>(i * 4 * sizeof(float)));
        vao_.setDivisor(4 + i, 1);
    }
    instanceCount_ = count;
}

void Mesh::updateInstanceData(const math::Mat4* models, gl::GLsizei count) {
    if (count != instanceCount_) return;
    instanceVbo_.bind();
    instanceVbo_.setSubData(0, static_cast<gl::GLsizeiptr>(count) * 16 * sizeof(float), models);
}

void Mesh::clearInstanceData() {
    vao_.bind();
    for (gl::GLuint i = 4; i < 8; ++i) vao_.disableAttrib(i);
    instanceCount_ = 0;
}

void Mesh::draw() const {
    vao_.bind();
    if (instanceCount_ > 0) {
        if (indexCount_ > 0)
            gl::drawElementsInstanced(gl::Primitive::Triangles, indexCount_, indexType_, nullptr, instanceCount_);
        else
            gl::drawArraysInstanced(gl::Primitive::Triangles, 0, vertexCount_, instanceCount_);
    } else if (indexCount_ > 0) {
        gl::drawElements(gl::Primitive::Triangles, indexCount_, indexType_, nullptr);
    } else {
        gl::drawArrays(gl::Primitive::Triangles, 0, vertexCount_);
    }
}

namespace shapes {

MeshData boxData(const math::Vec3& h, const math::Vec3& color) {
    // 6 faces, 4 verts each. Standard layout: pos(3) normal(3) color(3) uv(2).
    // Axis vectors have one +/-1 component, so component-wise multiply with h
    // yields the right extent per axis.
    const math::Vec3 n[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const math::Vec3 u[6] = {{0,0,1},{0,0,1},{1,0,0},{1,0,0},{1,0,0},{1,0,0}};
    const math::Vec3 v[6] = {{0,1,0},{0,1,0},{0,0,1},{0,0,1},{0,1,0},{0,1,0}};
    // faces whose (u,v) frame winds the quad against its normal need reversed indices,
    // otherwise they are back-facing (culled) when viewed from outside (+X, +Y, -Z)
    const bool flip[6] = {true, false, true, false, false, true};
    MeshData data;
    data.vertices.reserve(24 * MeshData::kFloatsPerVertex);
    for (int f = 0; f < 6; ++f) {
        const unsigned int base = static_cast<unsigned int>(data.vertexCount());
        for (int i = 0; i < 4; ++i) {
            const float su = (i == 1 || i == 2) ? 1.f : -1.f;
            const float sv = (i >= 2) ? 1.f : -1.f;
            const math::Vec3 p = n[f] * h + (u[f] * h) * su + (v[f] * h) * sv;
            const float uv[2] = {su * 0.5f + 0.5f, sv * 0.5f + 0.5f};
            const float vert[MeshData::kFloatsPerVertex] = {
                p.x, p.y, p.z, n[f].x, n[f].y, n[f].z, color.x, color.y, color.z, uv[0], uv[1]};
            data.vertices.insert(data.vertices.end(), vert, vert + MeshData::kFloatsPerVertex);
        }
        if (flip[f])
            data.indices.insert(data.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
        else
            data.indices.insert(data.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return data;
}

MeshData quadData(float halfSize, const math::Vec3& color) {
    MeshData data;
    const float verts[4 * MeshData::kFloatsPerVertex] = {
        -halfSize, -halfSize, 0,  0, 0, 1,  color.x, color.y, color.z,  0, 0,
         halfSize, -halfSize, 0,  0, 0, 1,  color.x, color.y, color.z,  1, 0,
         halfSize,  halfSize, 0,  0, 0, 1,  color.x, color.y, color.z,  1, 1,
        -halfSize,  halfSize, 0,  0, 0, 1,  color.x, color.y, color.z,  0, 1,
    };
    data.vertices.assign(verts, verts + 4 * MeshData::kFloatsPerVertex);
    data.indices = {0, 1, 2, 0, 2, 3};
    return data;
}

void box(Mesh& mesh, const math::Vec3& h, const math::Vec3& color) {
    mesh.setMeshData(boxData(h, color));
}

void quad(Mesh& mesh, float halfSize, const math::Vec3& color) {
    mesh.setMeshData(quadData(halfSize, color));
}

} // namespace shapes

} // namespace fuhgl::render
