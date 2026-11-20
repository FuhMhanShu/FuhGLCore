#pragma once

/// @file material.hpp
/// @brief Material: a shader program plus texture bindings, bound in one call.

#include "../gl/program.hpp"
#include "../gl/texture.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace fuhgl::render {

/// @brief Program + texture bindings. bind() makes the program current and binds
/// all textures to their units. Pipelines set the standard uniforms
/// ("uModel", "uViewProj", "uCamPos", …) themselves before drawing.
class Material {
public:
    Material();
    ~Material();

    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
    Material(Material&&) noexcept;
    Material& operator=(Material&&) noexcept;

    /// @brief Creates the underlying program object. Compile/link shaders, then done.
    bool create();

    Program& program() { return program_; }
    const Program& program() const { return program_; }

    /// @brief Binds a texture to a sampler unit (takes effect on bind()).
    void setTexture(gl::GLuint unit, const Texture& texture);
    void setTexture(gl::GLuint unit, gl::GLuint textureId);
    void setTexture(gl::GLuint unit, gl::GLenum target, gl::GLuint textureId);

    /// @brief Makes the program current and binds all textures.
    void bind() const;

    /// @brief Clears all texture bindings.
    void clearTextures();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Program program_;
};

} // namespace fuhgl::render
