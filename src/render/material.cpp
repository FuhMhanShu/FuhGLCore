#include "fuhgl/render/material.hpp"
#include "fuhgl/gl/state.hpp"

namespace fuhgl::render {

struct Material::Impl {
    struct Binding {
        gl::GLuint unit;
        gl::GLenum target;
        gl::GLuint id;
    };
    std::vector<Binding> textures;
};

Material::Material() : impl_(std::make_unique<Impl>()) {}
Material::~Material() = default;
Material::Material(Material&&) noexcept = default;
Material& Material::operator=(Material&&) noexcept = default;

bool Material::create() { return program_.create(); }

void Material::setTexture(gl::GLuint unit, const Texture& texture) {
    setTexture(unit, static_cast<gl::GLenum>(texture.target()), texture.id());
}
void Material::setTexture(gl::GLuint unit, gl::GLuint textureId) {
    setTexture(unit, gl::GL_TEXTURE_2D, textureId);
}
void Material::setTexture(gl::GLuint unit, gl::GLenum target, gl::GLuint textureId) {
    for (auto& t : impl_->textures)
        if (t.unit == unit) { t.target = target; t.id = textureId; return; }
    impl_->textures.push_back({unit, target, textureId});
}

void Material::bind() const {
    program_.use();
    for (const auto& t : impl_->textures) {
        gl::activeTexture(gl::GL_TEXTURE0 + t.unit);
        gl::functions().glBindTexture(t.target, t.id);
    }
}

void Material::clearTextures() { impl_->textures.clear(); }

} // namespace fuhgl::render
