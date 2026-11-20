#include "fuhgl/gl/sync.hpp"
#include "fuhgl/gl/loader.hpp"

namespace fuhgl {

struct Query::Impl {
    gl::GLuint id = 0;
    gl::GLenum target = gl::GL_TIME_ELAPSED;
};

Query::Query(gl::GLenum target) : impl_(std::make_unique<Impl>()) {
    impl_->target = target;
    if (gl::isInitialized() && gl::functions().glGenQueries)
        gl::functions().glGenQueries(1, &impl_->id);
}
Query::~Query() {
    if (impl_ && impl_->id && gl::isInitialized() && gl::functions().glDeleteQueries)
        gl::functions().glDeleteQueries(1, &impl_->id);
}
Query::Query(Query&&) noexcept = default;
Query& Query::operator=(Query&&) noexcept = default;

gl::GLenum Query::target() const { return impl_->target; }
void Query::begin() { gl::functions().glBeginQuery(impl_->target, impl_->id); }
void Query::end() { gl::functions().glEndQuery(impl_->target); }
bool Query::resultAvailable() const {
    gl::GLuint64 v = 0;
    gl::functions().glGetQueryObjectui64v(impl_->id, gl::GL_QUERY_RESULT_AVAILABLE, &v);
    return v != 0;
}
gl::GLuint64 Query::result() const {
    gl::GLuint64 v = 0;
    gl::functions().glGetQueryObjectui64v(impl_->id, gl::GL_QUERY_RESULT, &v);
    return v;
}
void Query::recordTimestamp() { gl::functions().glQueryCounter(impl_->id, gl::GL_TIMESTAMP); }
gl::GLuint Query::id() const { return impl_->id; }

struct Fence::Impl {
    gl::GLsync sync = nullptr;
};

Fence::Fence() : impl_(std::make_unique<Impl>()) {
    if (gl::isInitialized() && gl::functions().glFenceSync)
        impl_->sync = gl::functions().glFenceSync(gl::GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}
Fence::~Fence() {
    if (impl_ && impl_->sync && gl::isInitialized() && gl::functions().glDeleteSync)
        gl::functions().glDeleteSync(impl_->sync);
}
Fence::Fence(Fence&&) noexcept = default;
Fence& Fence::operator=(Fence&&) noexcept = default;

gl::GLenum Fence::wait(gl::GLuint64 timeoutNs) const {
    if (!impl_->sync) return gl::GL_WAIT_FAILED;
    return gl::functions().glClientWaitSync(impl_->sync, gl::GL_SYNC_FLUSH_COMMANDS_BIT, timeoutNs);
}
gl::GLsync Fence::handle() const { return impl_->sync; }

} // namespace fuhgl
