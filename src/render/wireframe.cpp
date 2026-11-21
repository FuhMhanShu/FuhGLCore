// Wireframe renderer: pure CPU edge extraction + one batched GL_LINES draw call.

#include "fuhgl/render/wireframe.hpp"

#include "fuhgl/gl/loader.hpp"

#include <algorithm>
#include <array>
#include <set>

namespace fuhgl::render {

std::array<math::Vec3, 8> boxCorners(const math::Vec3& mn, const math::Vec3& mx) {
    return {{
        {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z},
    }};
}

const std::array<std::array<std::uint32_t, 2>, 12> kBoxEdgeIndices = {{
    {{0, 1}}, {{2, 3}}, {{4, 5}}, {{6, 7}}, // x edges
    {{0, 2}}, {{1, 3}}, {{4, 6}}, {{5, 7}}, // y edges
    {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}, // z edges
}};

std::vector<std::pair<std::uint32_t, std::uint32_t>> wireframeEdges(const MeshData& mesh) {
    std::set<std::pair<std::uint32_t, std::uint32_t>> unique;
    const auto& idx = mesh.indices;
    const auto addTri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        const std::uint32_t e[3][2] = {{a, b}, {b, c}, {c, a}};
        for (const auto& p : e) {
            std::uint32_t lo = p[0], hi = p[1];
            if (lo > hi) std::swap(lo, hi);
            unique.insert({lo, hi});
        }
    };
    if (!idx.empty()) {
        for (std::size_t i = 0; i + 2 < idx.size(); i += 3)
            addTri(idx[i], idx[i + 1], idx[i + 2]);
    } else {
        const std::size_t n = mesh.vertexCount();
        for (std::size_t i = 0; i + 2 < n; i += 3)
            addTri(static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(i + 1),
                   static_cast<std::uint32_t>(i + 2));
    }
    return {unique.begin(), unique.end()};
}

namespace {

constexpr const char* kVs = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uViewProj;
out vec3 vColor;
void main() {
    vColor = aColor;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)GLSL";

constexpr const char* kFs = R"GLSL(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)GLSL";

} // namespace

// Mat4 has no Vec3 overload; transform via homogeneous w=1.
static math::Vec3 xform(const math::Mat4& m, const math::Vec3& v) {
    const math::Vec4 r = m * math::Vec4{v.x, v.y, v.z, 1.f};
    return {r.x, r.y, r.z};
}

struct Wireframe::Impl {
    Program prog;
    VertexArray vao;
    Buffer vbo{Buffer::Target::Array};
    bool created = false;

    bool create() {
        if (created) return true;
        const auto& f = gl::functions();
        if (!f.glUseProgram || !f.glDrawArrays) return false;
        Shader vs(Shader::Type::Vertex), fs(Shader::Type::Fragment);
        if (!vs.create(Shader::Type::Vertex) || !vs.compile(kVs)) return false;
        if (!fs.create(Shader::Type::Fragment) || !fs.compile(kFs)) return false;
        if (!prog.create()) return false;
        prog.attach(vs);
        prog.attach(fs);
        if (!prog.link()) return false;
        if (!vao.create()) return false;
        vao.bind();
        if (!vbo.create(Buffer::Target::Array)) return false;
        vbo.bind();
        constexpr gl::GLsizei stride = 6 * 4;
        vao.setAttrib(0, 3, gl::GL_FLOAT, false, stride, 0);
        vao.setAttrib(1, 3, gl::GL_FLOAT, false, stride, 3 * 4);
        vao.enableAttrib(0);
        vao.enableAttrib(1);
        VertexArray::unbind();
        created = true;
        return true;
    }
};

Wireframe::Wireframe() : impl_(std::make_unique<Impl>()) {}
Wireframe::~Wireframe() = default;
Wireframe::Wireframe(Wireframe&&) noexcept = default;
Wireframe& Wireframe::operator=(Wireframe&&) noexcept = default;

bool Wireframe::create() { return impl_->create(); }
bool Wireframe::isCreated() const { return impl_->created; }

void Wireframe::clear() { verts_.clear(); }

void Wireframe::addLine(const math::Vec3& a, const math::Vec3& b, const math::Vec3& color) {
    const float v[2][6] = {
        {a.x, a.y, a.z, color.x, color.y, color.z},
        {b.x, b.y, b.z, color.x, color.y, color.z},
    };
    verts_.insert(verts_.end(), &v[0][0], &v[0][0] + 12);
}

void Wireframe::addBox(const math::Vec3& mn, const math::Vec3& mx, const math::Vec3& color) {
    const auto c = boxCorners(mn, mx);
    for (const auto& e : kBoxEdgeIndices)
        addLine(c[e[0]], c[e[1]], color);
}

void Wireframe::addBoundingBox(const Solid& s, const math::Mat4& model, const math::Vec3& color) {
    const auto c = boxCorners(s.aabbMin(), s.aabbMax());
    math::Vec3 t[8];
    for (int i = 0; i < 8; ++i) t[i] = xform(model, c[i]);
    for (const auto& e : kBoxEdgeIndices)
        addLine(t[e[0]], t[e[1]], color);
}

void Wireframe::addMesh(const MeshData& mesh, const math::Mat4& model, const math::Vec3& color) {
    const std::size_t nv = mesh.vertexCount();
    std::vector<math::Vec3> pos(nv);
    for (std::size_t i = 0; i < nv; ++i) {
        const float* v = mesh.vertices.data() + i * MeshData::kFloatsPerVertex;
        pos[i] = xform(model, math::Vec3{v[0], v[1], v[2]});
    }
    for (const auto& e : wireframeEdges(mesh))
        addLine(pos[e.first], pos[e.second], color);
}

void Wireframe::addSolid(const Solid& s, int resolution, const math::Mat4& model, const math::Vec3& color) {
    addMesh(s.buildMeshData(resolution, color), model, color);
}

void Wireframe::render(const Camera& camera, int width, int height) {
    if (!impl_->created || verts_.empty()) return;
    const auto& f = gl::functions();
    f.glViewport(0, 0, width, height);
    f.glEnable(gl::GL_DEPTH_TEST);
    f.glDepthFunc(gl::GL_LESS);
    f.glDisable(gl::GL_CULL_FACE);
    impl_->prog.use();
    impl_->prog.setUniform("uViewProj", camera.viewProj(static_cast<float>(width) / static_cast<float>(height)));
    impl_->vao.bind();
    impl_->vbo.bind();
    f.glBufferData(gl::GL_ARRAY_BUFFER, static_cast<gl::GLsizeiptr>(verts_.size() * 4), verts_.data(), gl::GL_STREAM_DRAW);
    f.glDrawArrays(gl::GL_LINES, 0, static_cast<gl::GLsizei>(verts_.size() / 6));
    VertexArray::unbind();
}

} // namespace fuhgl::render
