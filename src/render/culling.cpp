#include "fuhgl/render/culling.hpp"

#include "fuhgl/gl/loader.hpp"
#include "fuhgl/gl/state.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace fuhgl::render {

// std430 layout shared with the compute shader: mat4 model + vec4 sphere = 80 bytes
struct CullItem {
    float model[16];
    float sphere[4];
};
static_assert(sizeof(CullItem) == 80, "CullItem must match the std430 Item layout");

// Indirect command block: DrawElementsIndirectCommand (5 x uint) or DrawArraysIndirectCommand
// (4 x uint) followed by the atomic counter at cmd[5]; instanceCount is cmd[1] in both layouts.
static constexpr int kCmdCounterOffset = 20; // bytes
static constexpr int kCmdBufferBytes = 32;

constexpr const char* kCullComp = R"GLSL(#version 430 core
layout(local_size_x = 64) in;

struct Item { mat4 model; vec4 sphere; };
layout(std430, binding = 0) readonly buffer Items { Item items[]; };
layout(std430, binding = 1) writeonly buffer Models { mat4 models[]; };
layout(std430, binding = 2) buffer Cmd { uint cmd[6]; }; // [0..4] = indirect command, [5] = counter

uniform vec4 uPlanes[6]; // xyz = plane normal, w = d (Hessian normal form)
uniform uint uItemCount;

void main() {
    const uint i = gl_GlobalInvocationID.x;
    if (i >= uItemCount) return;
    const mat4 model = items[i].model;
    const vec4 sph = items[i].sphere;
    // world-space bounding sphere: center = model * c, radius scaled by max axis scale
    const vec3 c = (model * vec4(sph.xyz, 1.0)).xyz;
    const float s = max(length(model[0].xyz), max(length(model[1].xyz), length(model[2].xyz)));
    const float r = sph.w * s;
    bool visible = true;
    for (int p = 0; p < 6; ++p)
        if (dot(uPlanes[p].xyz, c) + uPlanes[p].w < -r) { visible = false; break; }
    if (visible) {
        const uint slot = atomicAdd(cmd[5], 1u);
        models[slot] = model;
        atomicMax(cmd[1], slot + 1u); // instanceCount of the indirect command
    }
}
)GLSL";

struct GpuCuller::Impl {};

GpuCuller::GpuCuller() : impl_(std::make_unique<Impl>()) {}
GpuCuller::~GpuCuller() = default;
GpuCuller::GpuCuller(GpuCuller&&) noexcept = default;
GpuCuller& GpuCuller::operator=(GpuCuller&&) noexcept = default;

bool GpuCuller::create(int maxInstances) {
    ready_ = false;
    if (maxInstances <= 0) return false;
    if (!gl::isInitialized() || !gl::version().atLeast(4, 3)) return false;
    if (!gl::functions().glDispatchCompute || !gl::functions().glShaderStorageBlockBinding) return false;

    maxInstances_ = maxInstances;
    instanceCount_ = 0;

    items_.setData(static_cast<gl::GLsizeiptr>(maxInstances) * sizeof(CullItem), nullptr, Buffer::Usage::DynamicDraw);
    models_.setData(static_cast<gl::GLsizeiptr>(maxInstances) * 64, nullptr, Buffer::Usage::DynamicDraw);
    std::vector<unsigned int> zeros(kCmdBufferBytes / 4, 0u);
    cmd_.setData(kCmdBufferBytes, zeros.data(), Buffer::Usage::DynamicDraw);

    Shader comp(Shader::Type::Compute);
    if (!comp.compile(kCullComp)) {
        std::fprintf(stderr, "GpuCuller: compute shader: %s\n", comp.infoLog().c_str());
        return false;
    }
    if (!prog_.create()) return false;
    prog_.attach(comp);
    if (!prog_.link()) {
        std::fprintf(stderr, "GpuCuller: link: %s\n", prog_.infoLog().c_str());
        return false;
    }
    prog_.bindShaderStorageBlock("Items", 0);
    prog_.bindShaderStorageBlock("Models", 1);
    prog_.bindShaderStorageBlock("Cmd", 2);
    ready_ = true;
    return true;
}

void GpuCuller::setMesh(const Mesh& mesh) {
    vertexCount_ = mesh.vertexCount();
    indexCount_ = mesh.indexCount();
    indexType_ = mesh.indexType();
    indexed_ = indexCount_ > 0;
    sphere_ = mesh.boundingSphere();
    // write the static part of the indirect command (instanceCount/counter stay 0)
    unsigned int cmd[5] = {0u, 0u, 0u, 0u, 0u};
    if (indexed_) {
        cmd[0] = static_cast<unsigned int>(indexCount_); // count
        cmd[2] = 0u;                                       // firstIndex
        cmd[3] = 0u;                                       // baseVertex
    } else {
        cmd[0] = static_cast<unsigned int>(vertexCount_); // count
        cmd[2] = 0u;                                       // first
    }
    cmd_.setSubData(0, indexed_ ? 20 : 16, cmd);
    // own VAO: mesh vertex data (attribs 0..3) + models buffer as instance mat4 (4..7)
    vao_.create();
    vao_.bind();
    mesh.vertexBuffer().bind();
    vao_.setAttrib(0, 3, gl::GL_FLOAT, false, MeshData::kVertexBytes, 0);
    vao_.setAttrib(1, 3, gl::GL_FLOAT, false, MeshData::kVertexBytes, 3 * sizeof(float));
    vao_.setAttrib(2, 3, gl::GL_FLOAT, false, MeshData::kVertexBytes, 6 * sizeof(float));
    vao_.setAttrib(3, 2, gl::GL_FLOAT, false, MeshData::kVertexBytes, 9 * sizeof(float));
    if (indexed_)
        vao_.setElementBuffer(mesh.indexBuffer());
    // the same buffer object is the SSBO (compute writes) and the GL_ARRAY_BUFFER source
    // for the instance attributes — bind it explicitly as GL_ARRAY_BUFFER here
    gl::functions().glBindBuffer(gl::GL_ARRAY_BUFFER, models_.id());
    constexpr gl::GLsizei kMat4Bytes = 16 * sizeof(float);
    for (gl::GLuint i = 0; i < 4; ++i) {
        vao_.setAttrib(4 + i, 4, gl::GL_FLOAT, false, kMat4Bytes, static_cast<gl::GLintptr>(i * 4 * sizeof(float)));
        vao_.setDivisor(4 + i, 1);
    }
}

void GpuCuller::setInstances(const math::Mat4* models, int count) {
    if (!ready_ || !models || count <= 0) return;
    if (count > maxInstances_) count = maxInstances_;
    instanceCount_ = count;
    std::vector<CullItem> items(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        std::memcpy(items[i].model, models[i].data(), sizeof(items[i].model));
        items[i].sphere[0] = sphere_.x;
        items[i].sphere[1] = sphere_.y;
        items[i].sphere[2] = sphere_.z;
        items[i].sphere[3] = sphere_.w;
    }
    items_.setData(static_cast<gl::GLsizeiptr>(count) * sizeof(CullItem), items.data(), Buffer::Usage::DynamicDraw);
}

void GpuCuller::cull(const math::Mat4& viewProj) {
    if (!ready_ || instanceCount_ <= 0) return;
    const math::Frustum f = math::Frustum::fromViewProj(viewProj);
    float planes[24];
    for (int p = 0; p < 6; ++p) {
        planes[p * 4 + 0] = f.planes[p].n.x;
        planes[p * 4 + 1] = f.planes[p].n.y;
        planes[p * 4 + 2] = f.planes[p].n.z;
        planes[p * 4 + 3] = f.planes[p].d;
    }
    // reset instanceCount (cmd[1]) and the atomic counter (cmd[5])
    const unsigned int zero = 0u;
    cmd_.setSubData(4, 4, &zero);
    cmd_.setSubData(kCmdCounterOffset, 4, &zero);

    prog_.use();
    prog_.setUniform("uPlanes", 6, planes);
    prog_.setUniform("uItemCount", static_cast<unsigned int>(instanceCount_));
    items_.bindBase(0);
    models_.bindBase(1);
    cmd_.bindBase(2);
    gl::dispatchCompute(static_cast<gl::GLuint>((instanceCount_ + 63) / 64), 1, 1);
    gl::memoryBarrier(gl::GL_COMMAND_BARRIER_BIT | gl::GL_SHADER_STORAGE_BARRIER_BIT |
                      gl::GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
}

void GpuCuller::draw() const {
    if (!ready_) return;
    vao_.bind();
    cmd_.bind(); // GL_DRAW_INDIRECT_BUFFER
    if (indexed_)
        gl::drawElementsIndirect(gl::Primitive::Triangles, indexType_, nullptr);
    else
        gl::drawArraysIndirect(gl::Primitive::Triangles, nullptr);
}

int GpuCuller::lastVisibleCount() {
    if (!ready_) return 0;
    unsigned int v = 0;
    cmd_.getSubData(kCmdCounterOffset, 4, &v);
    return static_cast<int>(v);
}

} // namespace fuhgl::render
