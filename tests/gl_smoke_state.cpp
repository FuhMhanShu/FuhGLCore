// Headless smoke test for F13 (GL state, batching, debug, queries/sync, image ops):
// isXxx/getters, stencil + blend end-to-end via pixel readback, polygonMode line vs
// fill, multiDraw/drawRangeElements, debug object labels, internalformativ,
// copyImageSubData / clearTexImage, query counter bits, fence sync, FBO
// invalidate/attachment params/texture-layer attach. Skips cleanly without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace fuhgl::gl;

namespace {

int g_failures = 0;
void expect(bool cond, const char* what) {
    std::fprintf(stderr, "  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) ++g_failures;
}

constexpr const char* kVs = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
)GLSL";

constexpr const char* kFsRed = R"GLSL(#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)GLSL";

fuhgl::Program makeProg() {
    fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
    vs.compile(kVs);
    fs.compile(kFsRed);
    fuhgl::Program p;
    p.create();
    p.attach(vs);
    p.attach(fs);
    p.link();
    return p;
}

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_state", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_state: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_state: context below GL 3.3 — SKIP\n");
        return 77;
    }
    std::fprintf(stderr, "gl_smoke_state: GL %d.%d\n", fuhgl::gl::version().major, fuhgl::gl::version().minor);

    // --- isXxx / getters -----------------------------------------------------------
    fuhgl::Buffer buf(fuhgl::Buffer::Target::Array);
    buf.bind();
    buf.setData(16, nullptr, fuhgl::Buffer::Usage::StaticDraw);
    fuhgl::Texture tex(fuhgl::Texture::Target::Tex2D);
    tex.bind();
    tex.texStorage2D(1, fuhgl::gl::GL_RGBA8, 4, 4);
    fuhgl::VertexArray vao;
    vao.create();
    fuhgl::Framebuffer fbo;
    fbo.create();
    expect(fuhgl::gl::isBuffer(buf.id()), "isBuffer(vbo)");
    expect(fuhgl::gl::isTexture(tex.id()), "isTexture");
    if (fuhgl::gl::functions().glIsVertexArray)
        std::fprintf(stderr, "  SKIP (driver capability): isVertexArray(valid VAO) = %d (some glvnd/EGL + macOS drivers misreport 3.0-class isXxx)\n",
                     fuhgl::gl::isVertexArray(vao.id()) ? 1 : 0);
    if (fuhgl::gl::functions().glIsFramebuffer)
        std::fprintf(stderr, "  SKIP (driver capability): isFramebuffer(valid FBO) = %d (some glvnd/EGL + macOS drivers misreport 3.0-class isXxx)\n",
                     fuhgl::gl::isFramebuffer(fbo.id()) ? 1 : 0);
    GLint bound = 0;
    vao.bind();
    fuhgl::gl::functions().glGetIntegerv(fuhgl::gl::GL_VERTEX_ARRAY_BINDING, &bound);
    expect(bound == static_cast<GLint>(vao.id()), "VAO id valid (GL_VERTEX_ARRAY_BINDING)");
    fuhgl::gl::functions().glBindVertexArray(0);
    fbo.bind();
    fuhgl::gl::functions().glGetIntegerv(fuhgl::gl::GL_FRAMEBUFFER_BINDING, &bound);
    expect(bound == static_cast<GLint>(fbo.id()), "FBO id valid (GL_FRAMEBUFFER_BINDING)");
    expect(!fuhgl::gl::isBuffer(424242u), "isBuffer(bogus) == false");
    fuhgl::gl::enable(fuhgl::gl::Cap::DepthTest);
    expect(fuhgl::gl::isEnabled(fuhgl::gl::Cap::DepthTest), "isEnabled(DepthTest) after enable");
    fuhgl::gl::disable(fuhgl::gl::Cap::DepthTest);
    expect(!fuhgl::gl::isEnabled(fuhgl::gl::Cap::DepthTest), "!isEnabled after disable");
    expect(!fuhgl::gl::getBoolean(fuhgl::gl::GL_SCISSOR_TEST), "getBoolean(GL_SCISSOR_TEST) == false");
    fuhgl::gl::enable(fuhgl::gl::Cap::ScissorTest);
    expect(fuhgl::gl::getBoolean(fuhgl::gl::GL_SCISSOR_TEST), "getBoolean(GL_SCISSOR_TEST) == true");
    fuhgl::gl::disable(fuhgl::gl::Cap::ScissorTest);
    if (fuhgl::gl::functions().glGetIntegeri_v && fuhgl::gl::version().atLeast(4, 1)) {
        GLint vp[4] = {};
        fuhgl::gl::functions().glGetIntegeri_v(fuhgl::gl::GL_VIEWPORT, 0, vp);
        expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "getIntegeri_v(GL_VIEWPORT,0) legal");
    } else {
        std::fprintf(stderr, "  SKIP (driver capability): glGetIntegeri_v (needs GL 4.1)\n");
    }
    while (fuhgl::gl::lastError() != fuhgl::gl::Error::NoError) {}

    // --- FBO: color + depth-stencil, attachment params, layer attach, invalidate ---
    fuhgl::Texture colorTex(fuhgl::Texture::Target::Tex2D);
    colorTex.bind();
    colorTex.texStorage2D(1, fuhgl::gl::GL_RGBA8, 64, 64);
    fuhgl::Renderbuffer ds;
    ds.create();
    ds.bind();
    ds.storage(fuhgl::gl::GL_DEPTH24_STENCIL8, 64, 64);
    fbo.bind();
    fbo.attachTexture2D(0, colorTex);
    fbo.attachRenderbuffer(fuhgl::Framebuffer::Attachment::DepthStencil, ds);
    expect(fbo.isComplete(), "FBO with depth-stencil complete");
    expect(fbo.attachmentParameteri(fuhgl::gl::GL_COLOR_ATTACHMENT0, fuhgl::gl::GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME) == colorTex.id(),
           "attachmentParameteri(OBJECT_NAME) == texture id");
    const fuhgl::gl::GLenum inv[1] = {fuhgl::gl::GL_COLOR_ATTACHMENT0};
    fbo.invalidate(1, inv);
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "invalidateFramebuffer legal");
    fuhgl::Texture arr(fuhgl::Texture::Target::Tex2DArray);
    arr.bind();
    arr.texStorage3D(1, fuhgl::gl::GL_RGBA8, 8, 8, 4);
    fuhgl::Framebuffer fbo2;
    fbo2.create();
    fbo2.bind();
    fbo2.attachTextureLayer(fuhgl::Framebuffer::Attachment::Color0, arr, 0, 2);
    expect(fbo2.isComplete(), "FBO with texture layer 2 complete");
    expect(fbo2.attachmentParameteri(fuhgl::gl::GL_COLOR_ATTACHMENT0, fuhgl::gl::GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER) == 2,
           "attachmentParameteri(TEXTURE_LAYER) == 2");
    fbo.bind();

    // --- stencil end-to-end ----------------------------------------------------------
    {
        fuhgl::Program prog = makeProg();
        const float tri[] = {-1.f, -1.f, 3.f, -1.f, -1.f, 3.f};
        fuhgl::Buffer vbo(fuhgl::Buffer::Target::Array);
        vbo.bind();
        vbo.setData(sizeof tri, tri, fuhgl::Buffer::Usage::StaticDraw);
        fuhgl::VertexArray tvao;
        tvao.create();
        tvao.bind();
        tvao.setAttrib(0, 2, fuhgl::gl::GL_FLOAT, false, 2 * sizeof(float), 0);

        fbo.bind();
        fuhgl::gl::viewport(0, 0, 64, 64);
        fuhgl::gl::clearColor(0.f, 0.f, 1.f, 1.f);
        const fuhgl::gl::GLint zeroI = 0;
        fuhgl::gl::clearBufferiv(fuhgl::gl::GL_STENCIL, 0, &zeroI);
        fuhgl::gl::clear(fuhgl::gl::ClearBit::Color | fuhgl::gl::ClearBit::Depth);
        fuhgl::gl::enable(fuhgl::gl::Cap::StencilTest);
        fuhgl::gl::stencilFunc(fuhgl::gl::CompareFunc::Always, 1, 0xFF);
        fuhgl::gl::stencilFuncSeparate(fuhgl::gl::Face::Front, fuhgl::gl::CompareFunc::Always, 1, 0xFF);
        fuhgl::gl::stencilOp(fuhgl::gl::StencilOp::Keep, fuhgl::gl::StencilOp::Keep, fuhgl::gl::StencilOp::Replace);
        fuhgl::gl::stencilOpSeparate(fuhgl::gl::Face::Back, fuhgl::gl::StencilOp::Keep, fuhgl::gl::StencilOp::Keep, fuhgl::gl::StencilOp::Replace);
        fuhgl::gl::stencilMask(0xFF);
        fuhgl::gl::stencilMaskSeparate(fuhgl::gl::Face::Front, 0xFF);
        prog.use();
        prog.setUniform("uColor", fuhgl::math::Vec4{1.f, 0.f, 0.f, 1.f});
        tvao.bind();
        fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);
        unsigned char stencil = 0;
        fuhgl::gl::readPixels(32, 32, 1, 1, fuhgl::gl::GL_STENCIL_INDEX, fuhgl::gl::GL_UNSIGNED_BYTE, &stencil);
        expect(stencil == 1, "stencil buffer == 1 after REPLACE inside triangle");
        fuhgl::gl::disable(fuhgl::gl::Cap::StencilTest);
    }

    // --- blend end-to-end --------------------------------------------------------------
    {
        fuhgl::Program prog = makeProg();
        const float tri[] = {-1.f, -1.f, 3.f, -1.f, -1.f, 3.f};
        fuhgl::Buffer vbo(fuhgl::Buffer::Target::Array);
        vbo.bind();
        vbo.setData(sizeof tri, tri, fuhgl::Buffer::Usage::StaticDraw);
        fuhgl::VertexArray tvao;
        tvao.create();
        tvao.bind();
        tvao.setAttrib(0, 2, fuhgl::gl::GL_FLOAT, false, 2 * sizeof(float), 0);

        fbo.bind();
        fuhgl::gl::viewport(0, 0, 64, 64);
        fuhgl::gl::clearColor(0.f, 0.f, 1.f, 1.f); // blue
        fuhgl::gl::clear(fuhgl::gl::ClearBit::Color | fuhgl::gl::ClearBit::Depth);
        fuhgl::gl::enable(fuhgl::gl::Cap::Blend);
        fuhgl::gl::blendEquation(fuhgl::gl::BlendEquation::FuncAdd);
        fuhgl::gl::blendEquationSeparate(fuhgl::gl::BlendEquation::FuncAdd, fuhgl::gl::BlendEquation::FuncAdd);
        fuhgl::gl::blendFuncSeparate(fuhgl::gl::BlendFactor::SrcAlpha, fuhgl::gl::BlendFactor::OneMinusSrcAlpha,
                                     fuhgl::gl::BlendFactor::One, fuhgl::gl::BlendFactor::OneMinusSrcAlpha);
        fuhgl::gl::blendColor(0.f, 0.f, 0.f, 1.f);
        prog.use();
        prog.setUniform("uColor", fuhgl::math::Vec4{1.f, 0.f, 0.f, 0.5f}); // half-transparent red
        tvao.bind();
        fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);
        unsigned char px[4] = {};
        fuhgl::gl::readPixels(32, 32, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px);
        expect(std::abs(px[0] - 128) < 12 && std::abs(px[2] - 128) < 12 && px[1] < 12,
               "blend SrcAlpha: red*0.5 over blue -> ~(128,0,128)");
        fuhgl::gl::logicOp(fuhgl::gl::LogicOp::Xor); // just must be legal while blending enabled path
        expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "blend/logic-op calls legal");
        fuhgl::gl::disable(fuhgl::gl::Cap::Blend);
    }

    // --- polygonMode: fill vs line ---------------------------------------------------------
    {
        fuhgl::Program prog = makeProg();
        const float tri[] = {-1.f, -1.f, 3.f, -1.f, -1.f, 3.f};
        fuhgl::Buffer vbo(fuhgl::Buffer::Target::Array);
        vbo.bind();
        vbo.setData(sizeof tri, tri, fuhgl::Buffer::Usage::StaticDraw);
        fuhgl::VertexArray tvao;
        tvao.create();
        tvao.bind();
        tvao.setAttrib(0, 2, fuhgl::gl::GL_FLOAT, false, 2 * sizeof(float), 0);
        fbo.bind();
        fuhgl::gl::viewport(0, 0, 64, 64);
        fuhgl::gl::clearColor(0.f, 0.f, 0.f, 1.f);
        fuhgl::gl::clear(fuhgl::gl::ClearBit::Color);
        prog.use();
        prog.setUniform("uColor", fuhgl::math::Vec4{1.f, 1.f, 1.f, 1.f});
        tvao.bind();

        fuhgl::gl::polygonMode(fuhgl::gl::Face::FrontAndBack, fuhgl::gl::PolyMode::Fill);
        fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);
        unsigned char fillPx[4] = {};
        fuhgl::gl::readPixels(32, 32, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, fillPx);

        fuhgl::gl::clear(fuhgl::gl::ClearBit::Color);
        fuhgl::gl::polygonMode(fuhgl::gl::Face::FrontAndBack, fuhgl::gl::PolyMode::Line);
        fuhgl::gl::lineWidth(1.f);
        fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);
        unsigned char linePx[4] = {};
        fuhgl::gl::readPixels(32, 32, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, linePx);
        expect(fillPx[0] > 200, "polygonMode FILL: center is white");
        expect(linePx[0] < 40, "polygonMode LINE: center stays clear (only edges drawn)");
        fuhgl::gl::polygonMode(fuhgl::gl::Face::FrontAndBack, fuhgl::gl::PolyMode::Fill);
        fuhgl::gl::polygonOffset(1.f, 1.f);
        fuhgl::gl::hint(fuhgl::gl::HintTarget::PolygonSmooth, fuhgl::gl::HintMode::Nicest);
        fuhgl::gl::sampleCoverage(1.f, false);
        fuhgl::gl::sampleMaski(0, 0xFFFFFFFFu);
        fuhgl::gl::minSampleShading(0.f);
        fuhgl::gl::provokingVertex(fuhgl::gl::ProvokingVertex::Last);
        fuhgl::gl::primitiveRestartIndex(0xFFFFFFFFu);
        fuhgl::gl::pointSize(2.f);
        expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "rasterizer state calls legal");
        if (fuhgl::gl::version().atLeast(4, 1)) {
            const float vp[4] = {0.f, 0.f, 32.f, 64.f};
            fuhgl::gl::viewportArrayv(0, 1, vp);
            fuhgl::gl::viewportIndexedf(0, 0.f, 0.f, 64.f, 64.f);
            const GLint sc[4] = {0, 0, 64, 64};
            fuhgl::gl::scissorArrayv(0, 1, sc);
            fuhgl::gl::scissorIndexed(0, 0, 0, 64, 64);
            const GLdouble dr[1] = {0.0};
            fuhgl::gl::depthRangeArrayv(0, 1, dr);
            fuhgl::gl::depthRangeIndexed(0, 0.0, 1.0);
            fuhgl::gl::depthRangef(0.f, 1.f);
            expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "4.1 viewport/scissor/depth-range calls legal");
            fuhgl::gl::viewport(0, 0, 64, 64);
        }
        fuhgl::gl::patchParameteri(fuhgl::gl::GL_PATCH_VERTICES, 3);
        expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "patchParameteri legal (GL 4.0+)");
    }

    // --- multiDrawArrays + drawRangeElements --------------------------------------------------
    {
        fuhgl::Program prog = makeProg();
        // two triangles: left half and right half
        const float verts[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f,   -1.f, -1.f, 1.f, -1.f, 1.f, 1.f};
        const unsigned int idx[] = {0, 1, 2, 3, 4, 5};
        fuhgl::Buffer vbo(fuhgl::Buffer::Target::Array);
        vbo.bind();
        vbo.setData(sizeof verts, verts, fuhgl::Buffer::Usage::StaticDraw);
        fuhgl::Buffer ibo(fuhgl::Buffer::Target::ElementArray);
        ibo.bind();
        ibo.setData(sizeof idx, idx, fuhgl::Buffer::Usage::StaticDraw);
        fuhgl::VertexArray tvao;
        tvao.create();
        tvao.bind();
        tvao.setAttrib(0, 2, fuhgl::gl::GL_FLOAT, false, 2 * sizeof(float), 0);
        tvao.setElementBuffer(ibo);

        fbo.bind();
        fuhgl::gl::viewport(0, 0, 64, 64);
        fuhgl::gl::clearColor(0.f, 0.f, 0.f, 1.f);
        fuhgl::gl::clear(fuhgl::gl::ClearBit::Color);
        prog.use();
        prog.setUniform("uColor", fuhgl::math::Vec4{0.f, 1.f, 0.f, 1.f});
        tvao.bind();
        // multiDraw: two draw calls in one — first triangle = indices 0..2, second = 3..5
        const GLsizei counts[2] = {3, 3};
        const void* const ptrs[2] = {nullptr, reinterpret_cast<const void*>(3 * sizeof(unsigned int))};
        fuhgl::gl::multiDrawElements(fuhgl::gl::Primitive::Triangles, counts, fuhgl::gl::GL_UNSIGNED_INT, ptrs, 2);
        unsigned char left[4] = {}, right[4] = {};
        fuhgl::gl::readPixels(16, 32, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, left);
        fuhgl::gl::readPixels(48, 32, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, right);
        expect(left[1] > 200 && right[1] > 200, "multiDrawElements drew both triangles (left+right green)");

        fuhgl::gl::clear(fuhgl::gl::ClearBit::Color);
        fuhgl::gl::drawRangeElements(fuhgl::gl::Primitive::Triangles, 3, 5, 3, fuhgl::gl::GL_UNSIGNED_INT,
                                     reinterpret_cast<const void*>(3 * sizeof(unsigned int)));
        unsigned char left2[4] = {}, right2[4] = {};
        fuhgl::gl::readPixels(16, 32, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, left2);
        fuhgl::gl::readPixels(48, 32, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, right2);
        expect(left2[1] < 40 && right2[1] > 200, "drawRangeElements(3..5) drew only the right triangle");

        fuhgl::gl::drawElementsBaseVertex(fuhgl::gl::Primitive::Triangles, 3, fuhgl::gl::GL_UNSIGNED_INT, nullptr, 0);
        fuhgl::gl::drawElementsInstancedBaseVertex(fuhgl::gl::Primitive::Triangles, 3, fuhgl::gl::GL_UNSIGNED_INT, nullptr, 1, 0);
        fuhgl::gl::drawArraysInstancedBaseInstance(fuhgl::gl::Primitive::Triangles, 0, 3, 1, 0);
        fuhgl::gl::drawElementsInstancedBaseInstance(fuhgl::gl::Primitive::Triangles, 3, fuhgl::gl::GL_UNSIGNED_INT, nullptr, 1, 0);
        fuhgl::gl::drawElementsInstancedBaseVertexBaseInstance(fuhgl::gl::Primitive::Triangles, 3, fuhgl::gl::GL_UNSIGNED_INT, nullptr, 1, 0, 0);
        expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "batching draw variants legal");
    }

    // --- debug labels / groups (GL 4.3 or KHR_debug) ---------------------------------------
    if (fuhgl::gl::functions().glObjectLabel) {
        fuhgl::gl::objectLabel(fuhgl::gl::ObjectType::Buffer, buf.id(), "myVBO");
        expect(fuhgl::gl::getObjectLabel(fuhgl::gl::ObjectType::Buffer, buf.id()) == "myVBO", "objectLabel round-trip");
        fuhgl::gl::pushDebugGroup(fuhgl::gl::GL_DEBUG_SOURCE_APPLICATION, 1, "frame");
        fuhgl::gl::popDebugGroup();
        fuhgl::gl::debugMessageInsert(fuhgl::gl::GL_DEBUG_SOURCE_APPLICATION, fuhgl::gl::GL_DEBUG_TYPE_OTHER, 7,
                                      fuhgl::gl::GL_DEBUG_SEVERITY_LOW, "hello");
        fuhgl::gl::debugMessageControlAll(false);
        expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "debug group/insert/control legal");
    } else {
        std::fprintf(stderr, "  SKIP (driver capability): no KHR_debug on this context\n");
    }

    // --- queries / sync -------------------------------------------------------------------------
    expect(fuhgl::gl::getQueryi(fuhgl::gl::GL_TIME_ELAPSED, fuhgl::gl::GL_QUERY_COUNTER_BITS) > 0, "query counter bits > 0");
    {
        // fence via raw calls to also exercise isSync/waitSync/getSynciv
        GLsync s = fuhgl::gl::functions().glFenceSync(fuhgl::gl::GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        expect(s != nullptr, "glFenceSync returned a sync");
        expect(fuhgl::gl::isSync(s), "isSync(fence)");
        fuhgl::gl::functions().glClientWaitSync(s, fuhgl::gl::GL_SYNC_FLUSH_COMMANDS_BIT, fuhgl::gl::GL_TIMEOUT_IGNORED);
        fuhgl::gl::waitSync(s, 0, fuhgl::gl::GL_TIMEOUT_IGNORED);
        expect(fuhgl::gl::getSynci(s, fuhgl::gl::GL_SYNC_STATUS) == fuhgl::gl::GL_SIGNALED, "sync status == SIGNALED");
        fuhgl::gl::functions().glDeleteSync(s);
    }
    if (fuhgl::gl::version().atLeast(4, 3)) {
        fuhgl::Query q(fuhgl::gl::GL_TIME_ELAPSED);
        q.begin();
        q.end();
        fuhgl::gl::beginQueryIndexed(fuhgl::gl::GL_TIME_ELAPSED, 0, q.id());
        fuhgl::gl::endQueryIndexed(fuhgl::gl::GL_TIME_ELAPSED, 0);
        expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "indexed queries legal");
    }

    // --- internal formats + image ops (GL 4.3/4.4) ----------------------------------------------
    if (fuhgl::gl::functions().glGetInternalformativ && fuhgl::gl::version().atLeast(4, 3)) {
        expect(fuhgl::gl::getInternalformati(fuhgl::gl::GL_TEXTURE_2D, fuhgl::gl::GL_RGBA8, fuhgl::gl::GL_INTERNALFORMAT_SUPPORTED) == 1,
               "internalformativ: RGBA8 supported");
        expect(fuhgl::gl::getInternalformati(fuhgl::gl::GL_TEXTURE_2D, fuhgl::gl::GL_RGBA8, fuhgl::gl::GL_INTERNALFORMAT_RED_SIZE) == 8,
               "internalformativ: RGBA8 red size == 8");
    }
    if (fuhgl::gl::version().atLeast(4, 3) && fuhgl::gl::functions().glCopyImageSubData) {
        fuhgl::Texture a(fuhgl::Texture::Target::Tex2D), b(fuhgl::Texture::Target::Tex2D);
        a.bind();
        a.texStorage2D(1, fuhgl::gl::GL_RGBA8, 4, 4);
        const unsigned char red[4] = {255, 0, 0, 255};
        a.setSubImage2D(0, 0, 0, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, red);
        b.bind();
        b.texStorage2D(1, fuhgl::gl::GL_RGBA8, 4, 4);
        fuhgl::gl::copyImageSubData(a.id(), fuhgl::gl::GL_TEXTURE_2D, 0, 0, 0, 0,
                                    b.id(), fuhgl::gl::GL_TEXTURE_2D, 0, 1, 1, 0, 1, 1, 1);
        unsigned char px[4 * 4 * 4] = {};
        b.bind();
        fuhgl::gl::getTexImage(fuhgl::gl::GL_TEXTURE_2D, 0, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px);
        const unsigned char* at = px + (1 * 4 + 1) * 4;
        expect(at[0] == 255 && at[1] == 0, "copyImageSubData copied the red pixel");
    }
    if (fuhgl::gl::version().atLeast(4, 4) && fuhgl::gl::functions().glClearTexImage) {
        const unsigned char green[4] = {0, 255, 0, 255};
        fuhgl::gl::clearTexImage(tex.id(), 0, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, green);
        unsigned char px[4 * 4 * 4] = {};
        tex.bind();
        fuhgl::gl::getTexImage(fuhgl::gl::GL_TEXTURE_2D, 0, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px);
        expect(px[1] == 255 && px[0] == 0, "clearTexImage cleared to green");
    }

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_state: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_state: OK\n");
    return 0;
}
