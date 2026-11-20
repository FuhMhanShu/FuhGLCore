#include "fuhgl/gl/sampler.hpp"
#include "fuhgl/gl/loader.hpp"

namespace fuhgl {

struct Sampler::Impl {
    gl::GLuint id = 0;
};

Sampler::Sampler() : impl_(std::make_unique<Impl>()) {}
Sampler::~Sampler() {
    if (impl_ && impl_->id && gl::isInitialized() && gl::functions().glDeleteSamplers)
        gl::functions().glDeleteSamplers(1, &impl_->id);
}
Sampler::Sampler(Sampler&&) noexcept = default;
Sampler& Sampler::operator=(Sampler&&) noexcept = default;

bool Sampler::create() {
    if (impl_->id && gl::isInitialized() && gl::functions().glDeleteSamplers)
        gl::functions().glDeleteSamplers(1, &impl_->id);
    impl_->id = 0;
    if (!gl::isInitialized() || !gl::functions().glGenSamplers) return false;
    gl::functions().glGenSamplers(1, &impl_->id);
    return impl_->id != 0;
}

void Sampler::bind(gl::GLuint unit) const { gl::functions().glBindSampler(unit, impl_->id); }
void Sampler::unbind(gl::GLuint unit) { gl::functions().glBindSampler(unit, 0); }

void Sampler::setParameteri(gl::GLenum pname, gl::GLint param) const {
    gl::functions().glSamplerParameteri(impl_->id, pname, param);
}
void Sampler::setParameterf(gl::GLenum pname, gl::GLfloat param) const {
    gl::functions().glSamplerParameterf(impl_->id, pname, param);
}
void Sampler::setBorderColor(float r, float g, float b, float a) const {
    const float c[4] = {r, g, b, a};
    gl::functions().glSamplerParameterf(impl_->id, gl::GL_BORDER_COLOR, c[0]);
    // glSamplerParameterfv is not in the curated table; set via 4f through procAddress if present
    using PFN = void (*)(gl::GLuint, gl::GLenum, const gl::GLfloat*);
    if (auto p = reinterpret_cast<PFN>(gl::procAddress("glSamplerParameterfv")))
        p(impl_->id, gl::GL_BORDER_COLOR, c);
}

gl::GLuint Sampler::id() const { return impl_->id; }

} // namespace fuhgl
