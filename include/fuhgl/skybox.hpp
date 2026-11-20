#pragma once

/// @file skybox.hpp
/// @brief Skybox: cubemap on a unit cube, drawn behind everything via the
/// gl_Position = pos.xyww depth trick (renders at the far plane).

#include "cam.hpp"
#include "gl/program.hpp"
#include "gl/shader.hpp"
#include "gl/texture.hpp"
#include "gl/vao.hpp"

#include <memory>

namespace fuhgl {

/// @brief Skybox renderer. create() builds the cube VAO + shader program + cube texture.
/// Then either loadFaces() (6 images) or makeGradient() (procedural), and draw() each frame.
class Skybox {
public:
    Skybox();
    ~Skybox();

    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;
    Skybox(Skybox&&) noexcept;
    Skybox& operator=(Skybox&&) noexcept;

    /// @brief Creates the cube texture, shader program and unit-cube VAO.
    bool create();

    /// @brief Uploads 6 cube faces (PosX, NegX, PosY, NegY, PosZ, NegZ), each size x size.
    bool loadFaces(const void* const faces[6], gl::GLsizei size,
                   gl::GLenum internalFormat = gl::GL_RGB8,
                   gl::GLenum format = gl::GL_RGB,
                   gl::GLenum type = gl::GL_UNSIGNED_BYTE);

    /// @brief Procedural vertical gradient sky (no image files needed).
    void makeGradient(const math::Vec3& topColor, const math::Vec3& horizonColor, gl::GLsizei size = 256);

    /// @brief Draws the skybox. Enables the far-plane depth trick internally and restores
    /// depth func/mask afterwards. Call before drawing scene geometry.
    void draw(const Camera& camera, float aspect) const;

    Texture& texture() { return texture_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Texture texture_{Texture::Target::Cube};
};

} // namespace fuhgl
