#include "fuhgl/render/graph.hpp"

#include "fuhgl/gl/state.hpp"

#include <cstdio>
#include <unordered_map>

namespace fuhgl::render {

struct RenderGraph::Impl {
    struct Resource {
        std::string name;
        bool external = false;
        const Texture* tex = nullptr; // external
        int w = 0, h = 0;
        gl::GLenum internal = 0;
        Texture owned{Texture::Target::Tex2D};
    };
    struct Pass {
        std::string name;
        std::vector<std::string> inputs, outputs;
        PassFn fn;
        bool feedback = false;
        bool enabled = true;
        Framebuffer fbo;
    };
    std::vector<Resource> resources;
    std::vector<Pass> passes;
    std::unordered_map<std::string, int> resIndex;
    std::unordered_map<std::string, int> passIndex;
    int baseW = 0, baseH = 0;

    int findResource(std::string_view name) const {
        const auto it = resIndex.find(std::string(name));
        return it == resIndex.end() ? -1 : it->second;
    }
    int findPass(std::string_view name) const {
        const auto it = passIndex.find(std::string(name));
        return it == passIndex.end() ? -1 : it->second;
    }
};

const Texture* RenderGraph::PassContext::texture(std::string_view name) const { return graph.texture(name); }

void RenderGraph::PassContext::bindInput(int unit, std::string_view name) const {
    const Texture* t = graph.texture(name);
    if (!t) return;
    gl::activeTexture(gl::GL_TEXTURE0 + unit);
    gl::functions().glBindTexture(static_cast<gl::GLenum>(t->target()), t->id());
}

Framebuffer& RenderGraph::PassContext::framebuffer() { return graph.impl_->passes[pass].fbo; }

RenderGraph::RenderGraph() : impl_(std::make_unique<Impl>()) {}
RenderGraph::~RenderGraph() = default;
RenderGraph::RenderGraph(RenderGraph&&) noexcept = default;
RenderGraph& RenderGraph::operator=(RenderGraph&&) noexcept = default;

int RenderGraph::addResource(std::string name, int width, int height, gl::GLenum internalFormat) {
    Impl::Resource r;
    r.name = name;
    r.w = width;
    r.h = height;
    r.internal = internalFormat;
    impl_->resIndex[name] = static_cast<int>(impl_->resources.size());
    impl_->resources.push_back(std::move(r));
    return static_cast<int>(impl_->resources.size()) - 1;
}

int RenderGraph::addExternal(std::string name, const Texture* tex, int width, int height) {
    Impl::Resource r;
    r.name = name;
    r.external = true;
    r.tex = tex;
    r.w = width;
    r.h = height;
    impl_->resIndex[name] = static_cast<int>(impl_->resources.size());
    impl_->resources.push_back(std::move(r));
    return static_cast<int>(impl_->resources.size()) - 1;
}

int RenderGraph::addPass(std::string name, std::vector<std::string> inputs, std::vector<std::string> outputs,
                         PassFn fn, bool feedback) {
    Impl::Pass p;
    p.name = name;
    p.inputs = std::move(inputs);
    p.outputs = std::move(outputs);
    p.fn = std::move(fn);
    p.feedback = feedback;
    impl_->passIndex[name] = static_cast<int>(impl_->passes.size());
    impl_->passes.push_back(std::move(p));
    passCount_ = static_cast<int>(impl_->passes.size());
    return passCount_ - 1;
}

void RenderGraph::setPassEnabled(std::string_view name, bool enabled) {
    const int i = impl_->findPass(name);
    if (i >= 0) impl_->passes[i].enabled = enabled;
}

const Texture* RenderGraph::texture(std::string_view name) const {
    const int i = impl_->findResource(name);
    if (i < 0) return nullptr;
    const Impl::Resource& r = impl_->resources[i];
    return r.external ? r.tex : (r.owned.id() ? &r.owned : nullptr);
}

bool RenderGraph::compile() {
    // validate resource references and build the pure pass defs
    std::vector<GraphPassDef> defs;
    defs.reserve(impl_->passes.size());
    for (const Impl::Pass& p : impl_->passes) {
        for (const std::string& r : p.inputs)
            if (impl_->findResource(r) < 0) {
                std::fprintf(stderr, "RenderGraph: pass '%s' reads unknown resource '%s'\n", p.name.c_str(), r.c_str());
                return false;
            }
        for (const std::string& r : p.outputs)
            if (impl_->findResource(r) < 0) {
                std::fprintf(stderr, "RenderGraph: pass '%s' writes unknown resource '%s'\n", p.name.c_str(), r.c_str());
                return false;
            }
        GraphPassDef d;
        d.name = p.name;
        d.inputs = p.inputs;
        d.outputs = p.outputs;
        d.feedback = p.feedback;
        defs.push_back(std::move(d));
    }
    if (!resolvePassOrder(defs, order_)) {
        std::fprintf(stderr, "RenderGraph: dependency cycle detected\n");
        return false;
    }
    // allocate owned resources
    for (Impl::Resource& r : impl_->resources) {
        if (r.external) continue;
        r.owned = Texture(Texture::Target::Tex2D);
        r.owned.bind();
        r.owned.texStorage2D(1, r.internal, r.w, r.h);
        r.owned.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR);
        r.owned.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
        r.owned.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
        r.owned.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    }
    // one FBO per pass with its outputs attached
    for (Impl::Pass& p : impl_->passes) {
        if (p.outputs.empty()) continue;
        p.fbo = Framebuffer();
        p.fbo.create();
        int w = 0, h = 0;
        p.fbo.bind();
        std::vector<gl::GLenum> bufs;
        for (std::size_t k = 0; k < p.outputs.size(); ++k) {
            const int ri = impl_->findResource(p.outputs[k]);
            const Impl::Resource& r = impl_->resources[ri];
            const Texture& t = r.external ? *r.tex : r.owned;
            p.fbo.attachTexture2D(static_cast<int>(k), t);
            bufs.push_back(gl::GL_COLOR_ATTACHMENT0 + static_cast<gl::GLenum>(k));
            if (k == 0) {
                w = r.external ? r.w : r.w;
                h = r.h;
            }
        }
        p.fbo.setSize(w, h);
        gl::drawBuffers(static_cast<gl::GLsizei>(bufs.size()), bufs.data());
        if (!p.fbo.isComplete()) {
            std::fprintf(stderr, "RenderGraph: FBO of pass '%s' incomplete (0x%x)\n", p.name.c_str(), p.fbo.status());
            Framebuffer::unbind();
            return false;
        }
        Framebuffer::unbind();
    }
    return true;
}

void RenderGraph::execute() {
    for (int pi : order_) {
        Impl::Pass& p = impl_->passes[pi];
        if (!p.enabled || !p.fn) continue;
        if (p.outputs.empty()) {
            // pass without render targets: the callback binds its own target (e.g. the
            // pipeline output) — run it with the graph base size
            PassContext ctx{*this, pi, impl_->baseW, impl_->baseH};
            p.fn(ctx);
            continue;
        }
        const int ri = impl_->findResource(p.outputs[0]);
        const Impl::Resource& r = impl_->resources[ri];
        p.fbo.bind();
        {
            std::vector<gl::GLenum> bufs;
            for (std::size_t k = 0; k < p.outputs.size(); ++k)
                bufs.push_back(gl::GL_COLOR_ATTACHMENT0 + static_cast<gl::GLenum>(k));
            gl::drawBuffers(static_cast<gl::GLsizei>(bufs.size()), bufs.data());
        }
        gl::viewport(0, 0, r.w, r.h);
        PassContext ctx{*this, pi, r.w, r.h};
        p.fn(ctx);
        Framebuffer::unbind();
    }
}

void RenderGraph::resize(int width, int height) {
    if (width == impl_->baseW && height == impl_->baseH) return;
    impl_->baseW = width;
    impl_->baseH = height;
    for (Impl::Resource& r : impl_->resources)
        if (!r.external) {
            r.w = width;
            r.h = height;
        }
    compile(); // reallocates owned textures + FBOs (order unchanged)
}

} // namespace fuhgl::render
