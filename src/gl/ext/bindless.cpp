#include "fuhgl/gl/ext/bindless.hpp"

namespace fuhgl::gl::ext {

bool BindlessApi::load() {
    getTextureHandle = reinterpret_cast<decltype(getTextureHandle)>(gl::procAddress("glGetTextureHandleARB"));
    makeTextureHandleResident =
        reinterpret_cast<decltype(makeTextureHandleResident)>(gl::procAddress("glMakeTextureHandleResidentARB"));
    makeTextureHandleNonResident =
        reinterpret_cast<decltype(makeTextureHandleNonResident)>(gl::procAddress("glMakeTextureHandleNonResidentARB"));
    isTextureHandleResident =
        reinterpret_cast<decltype(isTextureHandleResident)>(gl::procAddress("glIsTextureHandleResidentARB"));
    uniformHandleui64 = reinterpret_cast<decltype(uniformHandleui64)>(gl::procAddress("glUniformHandleui64ARB"));
    programUniformHandleui64 =
        reinterpret_cast<decltype(programUniformHandleui64)>(gl::procAddress("glProgramUniformHandleui64ARB"));
    return getTextureHandle && makeTextureHandleResident && makeTextureHandleNonResident &&
           isTextureHandleResident && uniformHandleui64;
}

const BindlessApi& api() {
    static BindlessApi table = [] {
        BindlessApi t;
        t.load();
        return t;
    }();
    return table;
}

bool supported() {
    return gl::hasExtension("GL_ARB_bindless_texture") && api().getTextureHandle && api().makeTextureHandleResident &&
           api().makeTextureHandleNonResident && api().isTextureHandleResident && api().uniformHandleui64;
}

bool BindlessTexture::attach(const Texture& tex) {
    detach();
    if (!supported() || tex.id() == 0) return false;
    handle_ = api().getTextureHandle(tex.id());
    if (handle_ == 0) return false;
    api().makeTextureHandleResident(handle_);
    return api().isTextureHandleResident(handle_) != 0;
}

void BindlessTexture::detach() {
    if (handle_ != 0 && api().makeTextureHandleNonResident)
        api().makeTextureHandleNonResident(handle_);
    handle_ = 0;
}

bool BindlessTexture::isResident() const {
    return handle_ != 0 && api().isTextureHandleResident && api().isTextureHandleResident(handle_) != 0;
}

void setUniformHandle(GLint location, GLuint64 handle) {
    if (location >= 0 && api().uniformHandleui64)
        api().uniformHandleui64(location, handle);
}

void setUniformHandle(Program& prog, const char* name, GLuint64 handle) {
    const GLint loc = prog.uniformLocation(name);
    if (loc < 0) return;
    if (api().programUniformHandleui64)
        api().programUniformHandleui64(prog.id(), loc, handle);
    else {
        prog.use();
        setUniformHandle(loc, handle);
    }
}

} // namespace fuhgl::gl::ext
