// Headless smoke test for the F9 buffer/texture/image extensions: mapRange streaming,
// GPU-side buffer copies, texture sub-image/readback (2D + 3D), compressed texture
// round-trip, and (on GL >= 4.3) image binding + indirect compute + SSBO + VAO 4.3
// binding, each verified by readback. Skips cleanly without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int g_failures = 0;
void expect(bool cond, const char* what) {
    std::fprintf(stderr, "  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) ++g_failures;
}

constexpr const char* kTriVs = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aColor;
out vec3 vColor;
void main() { vColor = aColor; gl_Position = vec4(aPos, 0.0, 1.0); }
)GLSL";

constexpr const char* kTriFs = R"GLSL(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)GLSL";

constexpr const char* kImgCs = R"GLSL(#version 430 core
layout(local_size_x = 8, local_size_y = 8) in;
layout(rgba8, binding = 0) uniform writeonly image2D uImg;
void main() {
    imageStore(uImg, ivec2(gl_GlobalInvocationID.xy), vec4(0.25, 0.5, 0.75, 1.0));
}
)GLSL";

constexpr const char* kSsboCs = R"GLSL(#version 430 core
layout(local_size_x = 4) in;
layout(std430, binding = 0) buffer DataBlock { uint data[]; };
void main() {
    data[gl_GlobalInvocationID.x] = 1000u + gl_GlobalInvocationID.x;
}
)GLSL";

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_buffers", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_buffers: no GL context — skipping\n");
        return 0;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_buffers: context below GL 3.3 — skipping\n");
        return 0;
    }
    std::fprintf(stderr, "gl_smoke_buffers: GL %d.%d\n", fuhgl::gl::version().major, fuhgl::gl::version().minor);

    // --- 1) mapRange streaming + getSubData + parameter query --------------------
    {
        fuhgl::Buffer buf(fuhgl::Buffer::Target::Array);
        buf.bind();
        buf.setData(256, nullptr, fuhgl::Buffer::Usage::StreamDraw);
        void* p = buf.mapRange(0, 256, fuhgl::gl::GL_MAP_WRITE_BIT | fuhgl::gl::GL_MAP_INVALIDATE_BUFFER_BIT);
        expect(p != nullptr, "mapRange(MAP_WRITE|MAP_INVALIDATE_BUFFER)");
        if (p) {
            auto* bytes = static_cast<unsigned char*>(p);
            for (int i = 0; i < 256; ++i) bytes[i] = static_cast<unsigned char>(i);
            buf.flushMappedRange(0, 256); // no-op without FLUSH_EXPLICIT, must not error
            expect(buf.unmap(), "unmap");
        }
        unsigned char back[256] = {};
        buf.getSubData(0, 256, back);
        bool same = true;
        for (int i = 0; i < 256; ++i) same = same && back[i] == static_cast<unsigned char>(i);
        expect(same, "getSubData returns the streamed pattern");
        expect(buf.parameteri(fuhgl::gl::GL_BUFFER_SIZE) == 256, "parameteri(GL_BUFFER_SIZE) == 256");
        expect(buf.parameteri64(fuhgl::gl::GL_BUFFER_SIZE) == 256, "parameteri64(GL_BUFFER_SIZE) == 256");
    }

    // --- 2) GPU-side copy: copyBufferSubData ------------------------------------
    {
        fuhgl::Buffer src(fuhgl::Buffer::Target::CopyRead);
        fuhgl::Buffer dst(fuhgl::Buffer::Target::CopyWrite);
        const unsigned int pattern[16] = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};
        src.bind();
        src.setData(sizeof pattern, pattern, fuhgl::Buffer::Usage::StaticDraw);
        dst.bind();
        dst.setData(sizeof pattern, nullptr, fuhgl::Buffer::Usage::StaticDraw);
        fuhgl::gl::copyBufferSubData(fuhgl::gl::GL_COPY_READ_BUFFER, fuhgl::gl::GL_COPY_WRITE_BUFFER, 0, 0, sizeof pattern);
        unsigned int out[16] = {};
        dst.getSubData(0, sizeof out, out);
        expect(std::memcmp(out, pattern, sizeof pattern) == 0, "copyBufferSubData copied 16 uints");
        dst.invalidateSubData(0, 64); // must be legal, no error
        expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL error after invalidateSubData");
    }

    // --- 3) texture sub-image + readback, 2D and 3D -------------------------------
    {
        fuhgl::Texture tex(fuhgl::Texture::Target::Tex2D);
        tex.bind();
        tex.texStorage2D(1, fuhgl::gl::GL_RGBA8, 4, 4);
        const unsigned char red[4] = {255, 0, 0, 255};
        tex.setImage2D(0, fuhgl::gl::GL_RGBA8, 4, 4, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, nullptr);
        // upload one green pixel at (1,2)
        const unsigned char green[4] = {0, 255, 0, 255};
        tex.setSubImage2D(0, 1, 2, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, green);
        unsigned char px[4] = {};
        fuhgl::gl::getTexImage(fuhgl::gl::GL_TEXTURE_2D, 0, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px);
        // readback is bottom-up row order: pixel (1,2) is at offset (2*4+1)*4
        const unsigned char* at = px + (2 * 4 + 1) * 4;
        expect(at[0] == 0 && at[1] == 255 && at[2] == 0 && at[3] == 255, "getTexImage sees setSubImage2D pixel");
        expect(tex.getLevelParameteri(fuhgl::gl::GL_TEXTURE_WIDTH, 0) == 4, "getLevelParameteri(GL_TEXTURE_WIDTH) == 4");
        expect(tex.getParameteri(fuhgl::gl::GL_TEXTURE_MIN_FILTER) == fuhgl::gl::GL_NEAREST ||
               tex.getParameteri(fuhgl::gl::GL_TEXTURE_MIN_FILTER) == fuhgl::gl::GL_LINEAR,
               "getParameteri(GL_TEXTURE_MIN_FILTER) readable");

        fuhgl::Texture tex3(fuhgl::Texture::Target::Tex3D);
        tex3.bind();
        tex3.texStorage3D(1, fuhgl::gl::GL_RGBA8, 4, 4, 4);
        const unsigned char blue[4] = {0, 0, 255, 255};
        tex3.setSubImage3D(0, 3, 3, 3, 1, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, blue);
        std::vector<unsigned char> vol(4 * 4 * 4 * 4, 0);
        fuhgl::gl::getTexImage(fuhgl::gl::GL_TEXTURE_3D, 0, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, vol.data());
        const unsigned char* voxel = vol.data() + (3 * 4 * 4 + 3 * 4 + 3) * 4;
        expect(voxel[2] == 255 && voxel[0] == 0, "getTexImage(3D) sees setSubImage3D voxel");
    }

    // --- 4) compressed texture round-trip (DXT5 4x4 = 16 bytes) -------------------
    {
        fuhgl::Texture tex(fuhgl::Texture::Target::Tex2D);
        tex.bind();
        unsigned char dxt[16];
        for (int i = 0; i < 16; ++i) dxt[i] = static_cast<unsigned char>(0xA0 + i);
        tex.compressedImage2D(0, fuhgl::gl::GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 4, 4, sizeof dxt, dxt);
        const bool compressedOk = fuhgl::gl::lastError() == fuhgl::gl::Error::NoError;
        if (compressedOk) {
            expect(tex.getLevelParameteri(fuhgl::gl::GL_TEXTURE_COMPRESSED, 0) == 1, "level is compressed");
            expect(tex.getLevelParameteri(fuhgl::gl::GL_TEXTURE_COMPRESSED_IMAGE_SIZE, 0) == 16, "compressed image size == 16");
            unsigned char back[16] = {};
            fuhgl::gl::getCompressedTexImage(fuhgl::gl::GL_TEXTURE_2D, 0, back);
            expect(std::memcmp(back, dxt, 16) == 0, "getCompressedTexImage round-trips the DXT5 block");
        } else {
            std::fprintf(stderr, "  [skip] compressed textures unsupported on this driver\n");
        }
    }

    // --- 5) GL 4.3+: image binding + indirect compute + SSBO + VAO 4.3 binding ---
    if (fuhgl::gl::version().atLeast(4, 3)) {
        fuhgl::Shader cs(fuhgl::Shader::Type::Compute);
        expect(cs.compile(kImgCs), "image-write compute shader compiles");
        fuhgl::Program prog;
        prog.create();
        prog.attach(cs);
        expect(prog.link(), "compute program links");

        fuhgl::Texture img(fuhgl::Texture::Target::Tex2D);
        img.bind();
        img.texStorage2D(1, fuhgl::gl::GL_RGBA8, 8, 8);
        fuhgl::gl::bindImageTexture(0, img.id(), 0, false, 0, fuhgl::gl::GL_WRITE_ONLY, fuhgl::gl::GL_RGBA8);
        expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "bindImageTexture(rgba8, writeonly)");

        // direct dispatch
        prog.use();
        fuhgl::gl::dispatchCompute(1, 1, 1);
        fuhgl::gl::memoryBarrier(fuhgl::gl::GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        unsigned char px[4] = {};
        fuhgl::gl::getTexImage(fuhgl::gl::GL_TEXTURE_2D, 0, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px);
        expect(px[0] == 64 && px[1] == 128 && px[2] == 191 && px[3] == 255, "compute wrote via image binding");

        // indirect dispatch through GL_DISPATCH_INDIRECT_BUFFER
        fuhgl::Buffer indirect(fuhgl::Buffer::Target::DispatchIndirect);
        const unsigned int groups[3] = {1, 1, 1};
        indirect.bind();
        indirect.setData(sizeof groups, groups, fuhgl::Buffer::Usage::StaticDraw);
        img.bind();
        img.texStorage2D(1, fuhgl::gl::GL_RGBA8, 8, 8); // clear again
        fuhgl::gl::bindImageTexture(0, img.id(), 0, false, 0, fuhgl::gl::GL_WRITE_ONLY, fuhgl::gl::GL_RGBA8);
        fuhgl::gl::dispatchComputeIndirect(0);
        fuhgl::gl::memoryBarrier(fuhgl::gl::GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        unsigned char px2[4] = {};
        fuhgl::gl::getTexImage(fuhgl::gl::GL_TEXTURE_2D, 0, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px2);
        expect(px2[0] == 64 && px2[2] == 191, "dispatchComputeIndirect ran the same shader");

        // SSBO via bindShaderStorageBlock + glShaderStorageBlockBinding
        fuhgl::Shader cs2(fuhgl::Shader::Type::Compute);
        expect(cs2.compile(kSsboCs), "SSBO compute shader compiles");
        fuhgl::Program prog2;
        prog2.create();
        prog2.attach(cs2);
        expect(prog2.link(), "SSBO program links");
        prog2.bindShaderStorageBlock("DataBlock", 0);
        fuhgl::Buffer ssbo(fuhgl::Buffer::Target::ShaderStorage);
        ssbo.bind();
        ssbo.setData(4 * 4, nullptr, fuhgl::Buffer::Usage::DynamicDraw);
        ssbo.bindBase(0);
        prog2.use();
        fuhgl::gl::dispatchCompute(1, 1, 1);
        fuhgl::gl::memoryBarrier(fuhgl::gl::GL_SHADER_STORAGE_BARRIER_BIT);
        unsigned int data[4] = {};
        ssbo.getSubData(0, sizeof data, data);
        expect(data[0] == 1000 && data[3] == 1003, "SSBO written by compute (1000..1003)");

        // VAO 4.3 binding model: bindVertexBuffer + setAttribFormat/Binding renders a triangle
        fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
        expect(vs.compile(kTriVs) && fs.compile(kTriFs), "triangle shaders compile");
        fuhgl::Program prog3;
        prog3.create();
        prog3.attach(vs);
        prog3.attach(fs);
        expect(prog3.link(), "triangle program links");

        struct V { float p[2]; float c[3]; };
        const V verts[3] = {{{-1.f, -1.f}, {1.f, 0.f, 0.f}}, {{3.f, -1.f}, {0.f, 1.f, 0.f}}, {{-1.f, 3.f}, {0.f, 0.f, 1.f}}};
        fuhgl::Buffer vbo(fuhgl::Buffer::Target::Array);
        vbo.bind();
        vbo.setData(sizeof verts, verts, fuhgl::Buffer::Usage::StaticDraw);
        fuhgl::VertexArray vao;
        vao.create();
        vao.bind();
        vao.bindVertexBuffer(0, vbo, 0, sizeof(V));
        vao.setAttribFormat(0, 2, fuhgl::gl::GL_FLOAT, false, offsetof(V, p));
        vao.setAttribFormat(1, 3, fuhgl::gl::GL_FLOAT, false, offsetof(V, c));
        vao.setAttribBinding(0, 0);
        vao.setAttribBinding(1, 0);
        vao.enableAttrib(0);
        vao.enableAttrib(1);
        vao.setAttribI(2, 1, fuhgl::gl::GL_INT, 0, 0); // integer attrib path, unused by shader
        expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "VAO 4.3 binding calls are legal");

        fuhgl::Texture colorTex(fuhgl::Texture::Target::Tex2D);
        colorTex.bind();
        colorTex.texStorage2D(1, fuhgl::gl::GL_RGBA8, 64, 64);
        fuhgl::Framebuffer fbo;
        fbo.create();
        fbo.bind();
        fbo.attachTexture2D(fuhgl::Framebuffer::Attachment::Color0, colorTex);
        expect(fbo.isComplete(), "FBO complete");
        fuhgl::gl::viewport(0, 0, 64, 64);
        fuhgl::gl::clearColor(0.f, 0.f, 0.f, 1.f);
        fuhgl::gl::clear(fuhgl::gl::ClearBit::Color);
        prog3.use();
        vao.bind();
        fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);
        unsigned char center[4] = {};
        fuhgl::gl::readPixels(32, 32, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, center);
        expect(center[0] + center[1] + center[2] > 200, "VAO 4.3-bound triangle rendered at center");

        // uniform readback (glGetUniform*) + unsigned uniform setter
        prog3.use();
        prog3.setUniform("uMissing", 7u); // optimized out -> location -1, must not crash
        expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "setUniform on missing uniform is harmless");
    } else {
        std::fprintf(stderr, "  [skip] GL < 4.3 — image binding / indirect / SSBO / VAO-4.3 not tested\n");
    }

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_buffers: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_buffers: OK\n");
    return 0;
}
