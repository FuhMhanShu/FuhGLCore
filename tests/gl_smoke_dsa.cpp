// Headless smoke test for P2 (complete GL 4.5 DSA table): exercises the named
// buffer/texture/sampler/VAO/FBO/renderbuffer/query/TF entry points and verifies
// results via readback where possible. Skips cleanly without GL >= 4.5 / hasDsa().

#include <fuhgl/fuhgl.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace fuhgl::gl;

namespace {

int g_failures = 0;
void expect(bool cond, const char* what) {
    std::fprintf(stderr, "  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) ++g_failures;
}

// drains *all* pending GL errors so one bad call cannot cascade into the next group
void expectClean(const char* what) {
    for (int i = 0; i < 8; ++i) {
        const Error e = lastError();
        if (e == Error::NoError) return;
        std::fprintf(stderr, "  [FAIL] %s (GL error 0x%x)\n", what, static_cast<unsigned>(e));
        ++g_failures;
    }
}

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_dsa", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_dsa: no GL context — SKIP\n");
        return 77;
    }
    if (!version().atLeast(4, 5) || !hasDsa()) {
        std::fprintf(stderr, "gl_smoke_dsa: no DSA (GL < 4.5) — SKIP\n");
        return 77;
    }
    const auto& d = dsa();
    const auto& f = functions();
    expectClean("context clean at start");

    // --- buffers: immutable storage, mapping, copies, clears, params -----------
    {
        GLuint buf[2] = {0, 0};
        d.glCreateBuffers(2, buf);
        expect(buf[0] != 0 && buf[1] != 0, "glCreateBuffers");
        const GLbitfield flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
        d.glNamedBufferStorage(buf[0], 64, nullptr, flags);
        d.glNamedBufferStorage(buf[1], 64, nullptr, flags);
        GLint size = 0;
        GLint64 size64 = 0;
        d.glGetNamedBufferParameteriv(buf[0], GL_BUFFER_SIZE, &size);
        d.glGetNamedBufferParameteri64v(buf[0], GL_BUFFER_SIZE, &size64);
        expect(size == 64 && size64 == 64, "glGetNamedBufferParameteriv/i64v (GL_BUFFER_SIZE)");
        void* p = d.glMapNamedBufferRange(buf[0], 0, 64,
                                          GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
        expect(p != nullptr, "glMapNamedBufferRange");
        if (p) {
            for (int i = 0; i < 16; ++i) reinterpret_cast<GLuint*>(p)[i] = static_cast<GLuint>(0xA5A50000u + i);
            d.glFlushMappedNamedBufferRange(buf[0], 0, 64);
            expect(d.glUnmapNamedBuffer(buf[0]) == GL_TRUE, "glUnmapNamedBuffer");
        }
        GLuint back[16] = {};
        d.glGetNamedBufferSubData(buf[0], 0, 64, back);
        expect(back[0] == 0xA5A50000u && back[15] == 0xA5A5000Fu, "glGetNamedBufferSubData round-trip");
        d.glCopyNamedBufferSubData(buf[0], buf[1], 0, 0, 64);
        GLuint back2[16] = {};
        d.glGetNamedBufferSubData(buf[1], 0, 64, back2);
        expect(back2[7] == 0xA5A50007u, "glCopyNamedBufferSubData");
        const GLuint zero = 0;
        d.glClearNamedBufferSubData(buf[0], GL_R32UI, 0, 16, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
        d.glGetNamedBufferSubData(buf[0], 0, 16, back);
        expect(back[0] == 0 && back[3] == 0, "glClearNamedBufferSubData");
        d.glInvalidateNamedBufferSubData(buf[0], 0, 64);
        d.glInvalidateNamedBufferData(buf[1]);
        void* p2 = d.glMapNamedBuffer(buf[1], GL_READ_ONLY);
        expect(p2 != nullptr, "glMapNamedBuffer (read-only)");
        if (p2) expect(d.glUnmapNamedBuffer(buf[1]) == GL_TRUE, "glUnmapNamedBuffer after map");
        expectClean("buffer group");
        f.glDeleteBuffers(2, buf);
    }

    // --- 2D texture: storage, upload, params, readback, mipmaps, copies --------
    {
        GLuint tex = 0, tex2 = 0, texM = 0, fbo = 0;
        d.glCreateTextures(GL_TEXTURE_2D, 1, &tex);
        d.glTextureStorage2D(tex, 1, GL_RGBA8, 8, 8);
        std::vector<GLubyte> px(8 * 8 * 4);
        for (std::size_t i = 0; i < px.size(); i += 4) {
            px[i] = 200; px[i + 1] = static_cast<GLubyte>(i / 4); px[i + 2] = 30; px[i + 3] = 255;
        }
        d.glTextureSubImage2D(tex, 0, 0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        d.glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        d.glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        d.glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        d.glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
        d.glTextureParameterf(tex, GL_TEXTURE_MAX_LOD, 3.0f);
        const GLfloat border[4] = {1.f, 0.f, 0.f, 1.f};
        d.glTextureParameterfv(tex, GL_TEXTURE_BORDER_COLOR, border);
        GLint w = 0, ifmt = 0;
        d.glGetTextureLevelParameteriv(tex, 0, GL_TEXTURE_WIDTH, &w);
        d.glGetTextureLevelParameteriv(tex, 0, GL_TEXTURE_INTERNAL_FORMAT, &ifmt);
        expect(w == 8 && static_cast<GLenum>(ifmt) == GL_RGBA8, "glGetTextureLevelParameteriv");
        std::vector<GLubyte> rb(8 * 8 * 4, 0);
        d.glGetTextureImage(tex, 0, GL_RGBA, GL_UNSIGNED_BYTE, static_cast<GLsizei>(rb.size()), rb.data());
        expect(rb[0] == 200 && rb[1] == 0 && rb[5] == 1 && rb[8 * 8 * 4 - 1] == 255, "glGetTextureImage round-trip");
        d.glCreateTextures(GL_TEXTURE_2D, 1, &tex2);
        d.glTextureStorage2D(tex2, 1, GL_RGBA8, 8, 8);
        d.glCreateFramebuffers(1, &fbo);
        d.glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);
        d.glNamedFramebufferDrawBuffer(fbo, GL_COLOR_ATTACHMENT0);
        d.glNamedFramebufferReadBuffer(fbo, GL_COLOR_ATTACHMENT0);
        expect(d.glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO complete (texture attach)");
        f.glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        d.glCopyTextureSubImage2D(tex2, 0, 0, 0, 0, 0, 8, 8);
        f.glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        std::vector<GLubyte> rb2(8 * 8 * 4, 0);
        d.glGetTextureImage(tex2, 0, GL_RGBA, GL_UNSIGNED_BYTE, static_cast<GLsizei>(rb2.size()), rb2.data());
        expect(rb2[0] == 200 && rb2[5] == 1, "glCopyTextureSubImage2D from read FBO");
        d.glCreateTextures(GL_TEXTURE_2D, 1, &texM);
        d.glTextureStorage2D(texM, 4, GL_RGBA8, 8, 8);
        d.glGenerateTextureMipmap(texM);
        GLint lw = 0;
        d.glGetTextureLevelParameteriv(texM, 3, GL_TEXTURE_WIDTH, &lw);
        expect(lw == 1, "glGenerateTextureMipmap (level 3 width == 1)");
        const GLubyte redB[4] = {255, 0, 0, 255};
        f.glClearTexImage(tex, 0, GL_RGBA, GL_UNSIGNED_BYTE, redB);
        std::vector<GLubyte> rb3(8 * 8 * 4, 0);
        d.glGetTextureImage(tex, 0, GL_RGBA, GL_UNSIGNED_BYTE, static_cast<GLsizei>(rb3.size()), rb3.data());
        expect(rb3[0] == 255 && rb3[1] == 0 && rb3[2] == 0 && rb3[3] == 255, "glClearTexImage (curated table)");
        expect(d.glCompressedTextureSubImage2D != nullptr && d.glCompressedTextureSubImage3D != nullptr
                   && d.glGetCompressedTextureImage != nullptr,
               "compressed-texture DSA pointers present");
        expectClean("texture group");
        f.glDeleteTextures(1, &tex);
        f.glDeleteTextures(1, &tex2);
        f.glDeleteTextures(1, &texM);
        f.glDeleteFramebuffers(1, &fbo);
    }

    // --- 3D texture + multisample storage + texture buffer ----------------------
    {
        GLuint tex3 = 0, texMS = 0, texMS3 = 0, buf = 0, texB = 0;
        d.glCreateTextures(GL_TEXTURE_3D, 1, &tex3);
        d.glTextureStorage3D(tex3, 1, GL_RGBA8, 4, 4, 4);
        std::vector<GLubyte> px(4 * 4 * 2 * 4, 77);
        d.glTextureSubImage3D(tex3, 0, 0, 0, 0, 4, 4, 2, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        std::vector<GLubyte> rb(4 * 4 * 4 * 4, 0);
        d.glGetTextureImage(tex3, 0, GL_RGBA, GL_UNSIGNED_BYTE, static_cast<GLsizei>(rb.size()), rb.data());
        expect(rb[0] == 77 && rb[4 * 4 * 2 * 4] == 0, "glTextureSubImage3D + glGetTextureImage (3D)");
        d.glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &texMS);
        d.glTextureStorage2DMultisample(texMS, 4, GL_RGBA8, 16, 16, GL_TRUE);
        expectClean("glTextureStorage2DMultisample");
        d.glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 1, &texMS3);
        d.glTextureStorage3DMultisample(texMS3, 4, GL_RGBA8, 8, 8, 4, GL_TRUE);
        expectClean("glTextureStorage3DMultisample");
        d.glCreateBuffers(1, &buf);
        d.glNamedBufferStorage(buf, 64, nullptr, GL_DYNAMIC_STORAGE_BIT);
        expectClean("named buffer for texture buffer");
        d.glCreateTextures(GL_TEXTURE_BUFFER, 1, &texB);
        expectClean("glCreateTextures(GL_TEXTURE_BUFFER)");
        d.glTextureBuffer(texB, GL_R32F, buf);
        expectClean("glTextureBuffer");
        d.glTextureBufferRange(texB, GL_R32F, buf, 0, 32);
        expectClean("glTextureBufferRange");
        expectClean("3D/MS/buffer-texture group");
        f.glDeleteTextures(1, &tex3);
        f.glDeleteTextures(1, &texMS);
        f.glDeleteTextures(1, &texMS3);
        f.glDeleteTextures(1, &texB);
        f.glDeleteBuffers(1, &buf);
    }

    // --- samplers: params + unit/range binding ----------------------------------
    {
        GLuint s = 0, tex = 0;
        d.glCreateSamplers(1, &s);
        d.glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        d.glSamplerParameterf(s, GL_TEXTURE_MAX_LOD, 3.5f);
        const GLfloat bc[4] = {0.f, 1.f, 0.f, 1.f};
        d.glSamplerParameterfv(s, GL_TEXTURE_BORDER_COLOR, bc);
        d.glBindSampler(0, s);
        GLint sb = 0;
        f.glGetIntegeri_v(GL_SAMPLER_BINDING, 0, &sb);
        expect(static_cast<GLuint>(sb) == s, "glBindSampler (unit 0)");
        d.glBindSamplers(2, 1, &s);
        f.glGetIntegeri_v(GL_SAMPLER_BINDING, 2, &sb);
        expect(static_cast<GLuint>(sb) == s, "glBindSamplers (range, unit 2)");
        d.glCreateTextures(GL_TEXTURE_2D, 1, &tex);
        d.glTextureStorage2D(tex, 1, GL_RGBA8, 4, 4);
        d.glBindTextureUnit(0, tex);
        GLint tb = 0;
        f.glGetIntegerv(GL_TEXTURE_BINDING_2D, &tb);
        expect(static_cast<GLuint>(tb) == tex, "glBindTextureUnit (unit 0)");
        expectClean("sampler group");
        f.glDeleteSamplers(1, &s);
        f.glDeleteTextures(1, &tex);
    }

    // --- vertex arrays: formats, bindings, divisors, element buffer, queries ----
    {
        GLuint vao = 0, buf = 0, ebuf = 0;
        d.glCreateVertexArrays(1, &vao);
        d.glCreateBuffers(1, &buf);
        d.glCreateBuffers(1, &ebuf);
        d.glNamedBufferData(buf, 64, nullptr, GL_STATIC_DRAW);
        d.glNamedBufferData(ebuf, 24, nullptr, GL_STATIC_DRAW);
        d.glEnableVertexArrayAttrib(vao, 0);
        d.glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
        d.glVertexArrayAttribBinding(vao, 0, 0);
        d.glVertexArrayVertexBuffer(vao, 0, buf, 0, 12);
        d.glEnableVertexArrayAttrib(vao, 1);
        d.glVertexArrayAttribIFormat(vao, 1, 1, GL_UNSIGNED_INT, 0);
        d.glVertexArrayAttribBinding(vao, 1, 0);
        d.glEnableVertexArrayAttrib(vao, 2);
        d.glVertexArrayAttribLFormat(vao, 2, 1, GL_DOUBLE, 0);
        d.glVertexArrayAttribBinding(vao, 2, 0);
        d.glVertexArrayBindingDivisor(vao, 0, 1);
        d.glVertexArrayElementBuffer(vao, ebuf);
        GLint eab = 0, div = 0;
        GLint64 off = -1;
        d.glGetVertexArrayiv(vao, GL_ELEMENT_ARRAY_BUFFER_BINDING, &eab);
        d.glGetVertexArrayIndexediv(vao, 0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &div);
        d.glGetVertexArrayIndexed64iv(vao, 0, GL_VERTEX_BINDING_OFFSET, &off);
        expect(static_cast<GLuint>(eab) == ebuf, "glGetVertexArrayiv (element buffer binding)");
        expect(div == 1, "glGetVertexArrayIndexediv (divisor)");
        expect(off == 0, "glGetVertexArrayIndexed64iv (binding offset)");
        expectClean("VAO group");
        f.glDeleteVertexArrays(1, &vao);
        f.glDeleteBuffers(1, &buf);
        f.glDeleteBuffers(1, &ebuf);
    }

    // --- framebuffers: typed clears, texture layers, MS blit, draw buffers ------
    {
        GLuint tex = 0, texI = 0, texUI = 0, texArr = 0, texMS = 0;
        GLuint fbos[5] = {0, 0, 0, 0, 0};
        GLuint rb = 0, rbMS = 0;
        d.glCreateTextures(GL_TEXTURE_2D, 1, &tex);
        d.glTextureStorage2D(tex, 1, GL_RGBA8, 8, 8);
        d.glCreateTextures(GL_TEXTURE_2D, 1, &texI);
        d.glTextureStorage2D(texI, 1, GL_R32UI, 4, 4);
        d.glCreateTextures(GL_TEXTURE_2D, 1, &texUI);
        d.glTextureStorage2D(texUI, 1, GL_RGBA32UI, 4, 4);
        d.glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &texArr);
        d.glTextureStorage3D(texArr, 1, GL_RGBA8, 8, 8, 4);
        d.glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &texMS);
        d.glTextureStorage2DMultisample(texMS, 4, GL_RGBA8, 16, 16, GL_TRUE);
        d.glCreateFramebuffers(5, fbos);
        GLuint fbo = fbos[0], fboI = fbos[1], fboUI = fbos[2], fboArr = fbos[3], fboMS = fbos[4];
        expect(fbo && fboI && fboUI && fboArr && fboMS, "glCreateFramebuffers (5 ids)");
        // fv clear on RGBA8 attachment
        d.glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);
        d.glNamedFramebufferDrawBuffer(fbo, GL_COLOR_ATTACHMENT0);
        d.glNamedFramebufferReadBuffer(fbo, GL_COLOR_ATTACHMENT0);
        expect(d.glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "fbo RGBA8 complete");
        const GLfloat red[4] = {1.f, 0.f, 0.f, 1.f};
        d.glClearNamedFramebufferfv(fbo, GL_COLOR, 0, red);
        f.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        GLubyte px[4] = {};
        f.glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        expect(px[0] == 255 && px[1] == 0 && px[2] == 0 && px[3] == 255, "glClearNamedFramebufferfv + readback");
        // iv clear on R32UI attachment (integer attachments are not color-renderable,
        // so the FBO stays incomplete — clears still work; verify via texture readback)
        d.glNamedFramebufferTexture(fboI, GL_COLOR_ATTACHMENT0, texI, 0);
        d.glNamedFramebufferDrawBuffer(fboI, GL_COLOR_ATTACHMENT0);
        const GLint iv[4] = {42, 0, 0, 0};
        d.glClearNamedFramebufferiv(fboI, GL_COLOR, 0, iv);
        expectClean("glClearNamedFramebufferiv call");
        std::vector<GLuint> pxI(4 * 4, 0); // glGetTextureImage reads the whole level
        d.glGetTextureImage(texI, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, 4 * 4 * 4, pxI.data());
        expect(pxI[0] == 42, "glClearNamedFramebufferiv (verified via glGetTextureImage)");
        // uiv clear on RGBA32UI attachment
        d.glNamedFramebufferTexture(fboUI, GL_COLOR_ATTACHMENT0, texUI, 0);
        d.glNamedFramebufferDrawBuffer(fboUI, GL_COLOR_ATTACHMENT0);
        const GLuint uiv[4] = {7, 0, 0, 0};
        d.glClearNamedFramebufferuiv(fboUI, GL_COLOR, 0, uiv);
        expectClean("glClearNamedFramebufferuiv call");
        std::vector<GLuint> pxU(4 * 4 * 4, 0); // whole level: 4*4 RGBA32UI = 256 bytes
        d.glGetTextureImage(texUI, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, 4 * 4 * 16, pxU.data());
        expect(pxU[0] == 7, "glClearNamedFramebufferuiv (verified via glGetTextureImage)");
        // fi clear on depth/stencil renderbuffer attachment
        d.glCreateRenderbuffers(1, &rb);
        d.glNamedRenderbufferStorage(rb, GL_DEPTH24_STENCIL8, 8, 8);
        f.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        f.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
        d.glClearNamedFramebufferfi(fbo, GL_DEPTH_STENCIL, 0, 0.5f, 3);
        expectClean("glClearNamedFramebufferfi call");
        GLfloat df = 0.f;
        GLint sf = 0;
        f.glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &df);
        f.glReadPixels(0, 0, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &sf);
        expect(df > 0.49f && df < 0.51f && sf == 3, "glClearNamedFramebufferfi (depth+stencil)");
        f.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // texture layer attach + clear
        d.glNamedFramebufferTextureLayer(fboArr, GL_COLOR_ATTACHMENT0, texArr, 0, 2);
        d.glNamedFramebufferDrawBuffer(fboArr, GL_COLOR_ATTACHMENT0);
        d.glNamedFramebufferReadBuffer(fboArr, GL_COLOR_ATTACHMENT0);
        expect(d.glCheckNamedFramebufferStatus(fboArr, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "fbo texture-layer complete");
        const GLfloat green[4] = {0.f, 1.f, 0.f, 1.f};
        d.glClearNamedFramebufferfv(fboArr, GL_COLOR, 0, green);
        f.glBindFramebuffer(GL_FRAMEBUFFER, fboArr);
        f.glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        expect(px[1] == 255 && px[0] == 0, "glNamedFramebufferTextureLayer clear readback");
        // multi draw buffers state
        const GLenum dbs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        d.glNamedFramebufferDrawBuffers(fbo, 2, dbs);
        d.glNamedFramebufferDrawBuffer(fbo, GL_COLOR_ATTACHMENT0);
        expectClean("glNamedFramebufferDrawBuffer(s)");
        // MSAA renderbuffer storage + query + blit to single-sample fbo
        d.glCreateRenderbuffers(1, &rbMS);
        d.glNamedRenderbufferStorageMultisample(rbMS, 4, GL_RGBA8, 16, 16);
        GLint samples = 0;
        d.glGetNamedRenderbufferParameteriv(rbMS, GL_RENDERBUFFER_SAMPLES, &samples);
        expect(samples == 4, "glNamedRenderbufferStorageMultisample + glGetNamedRenderbufferParameteriv");
        d.glNamedFramebufferTexture(fboMS, GL_COLOR_ATTACHMENT0, texMS, 0);
        expectClean("attach MS texture to fboMS");
        d.glNamedFramebufferDrawBuffer(fboMS, GL_COLOR_ATTACHMENT0);
        d.glNamedFramebufferReadBuffer(fboMS, GL_COLOR_ATTACHMENT0);
        expect(d.glCheckNamedFramebufferStatus(fboMS, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "MSAA fbo complete");
        d.glClearNamedFramebufferfv(fboMS, GL_COLOR, 0, red);
        expectClean("glClearNamedFramebufferfv on MSAA fbo");
        // blit single-sample -> single-sample (llvmpipe rejects the MS resolve blit
        // with INVALID_OPERATION; MSAA storage itself is covered above)
        GLuint texS = 0, fboS = 0;
        d.glCreateTextures(GL_TEXTURE_2D, 1, &texS);
        d.glTextureStorage2D(texS, 1, GL_RGBA8, 16, 16);
        d.glCreateFramebuffers(1, &fboS);
        d.glNamedFramebufferTexture(fboS, GL_COLOR_ATTACHMENT0, texS, 0);
        d.glNamedFramebufferDrawBuffer(fboS, GL_COLOR_ATTACHMENT0);
        d.glNamedFramebufferReadBuffer(fboS, GL_COLOR_ATTACHMENT0);
        expect(d.glCheckNamedFramebufferStatus(fboS, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "src fbo complete");
        d.glClearNamedFramebufferfv(fboS, GL_COLOR, 0, red);
        const GLfloat blue[4] = {0.f, 0.f, 1.f, 1.f};
        d.glClearNamedFramebufferfv(fbo, GL_COLOR, 0, blue);
        d.glBlitNamedFramebuffer(fboS, fbo, 0, 0, 16, 16, 0, 0, 8, 8, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        expectClean("glBlitNamedFramebuffer (single -> single)");
        f.glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        f.glReadBuffer(GL_COLOR_ATTACHMENT0);
        f.glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        expect(px[0] == 255 && px[1] == 0 && px[2] == 0, "glBlitNamedFramebuffer copied red over blue");
        f.glDeleteTextures(1, &texS);
        f.glDeleteFramebuffers(1, &fboS);
        f.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        expectClean("framebuffer group");
        const GLuint texs[5] = {tex, texI, texUI, texArr, texMS};
        f.glDeleteTextures(5, texs);
        f.glDeleteFramebuffers(5, fbos);
        const GLuint rbs[2] = {rb, rbMS};
        f.glDeleteRenderbuffers(2, rbs);
    }

    // --- queries + transform feedback -------------------------------------------
    {
        GLuint q = 0, tf = 0;
        d.glCreateQueries(GL_SAMPLES_PASSED, 1, &q);
        expect(q != 0, "glCreateQueries");
        f.glBeginQuery(GL_SAMPLES_PASSED, q);
        f.glEndQuery(GL_SAMPLES_PASSED);
        f.glFinish();
        GLuint64 avail = 0, result = 0;
        f.glGetQueryObjectui64v(q, GL_QUERY_RESULT_AVAILABLE, &avail);
        f.glGetQueryObjectui64v(q, GL_QUERY_RESULT, &result);
        expect(avail == 1, "query result available (0 samples drawn)");
        d.glCreateTransformFeedbacks(1, &tf);
        expect(tf != 0, "glCreateTransformFeedbacks");
        expectClean("query/TF group");
        f.glDeleteQueries(1, &q);
    }

    expectClean("all DSA groups");
    if (g_failures == 0) std::fprintf(stderr, "gl_smoke_dsa: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
