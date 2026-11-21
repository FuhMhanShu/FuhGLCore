#pragma once

/// @file scene.hpp
/// @brief Plain data types for the render pipelines: lights, draw items, scene view.
/// Deliberately NOT a scene graph — just a flat list the pipeline iterates.

#include "../cam.hpp"
#include "../gl/types.hpp"
#include "../math.hpp"
#include "light.hpp"

#include <vector>

namespace fuhgl::render {

class Mesh;
class Material;

/// @brief One drawable object: mesh + material + model matrix.
struct DrawItem {
    const Mesh* mesh = nullptr;
    const Material* material = nullptr; ///< nullptr = pipeline default material
    math::Mat4 model{};                  ///< identity by default
};

/// @brief Everything a pipeline needs for one frame.
struct SceneView {
    const Camera* camera = nullptr;
    int width = 0;              ///< viewport size in pixels
    int height = 0;
    float aspect = 1.f;         ///< width / height
    math::Vec3 clearColor{0.05f, 0.05f, 0.08f};
    math::Vec3 ambient{0.06f, 0.06f, 0.07f};
    std::vector<DrawItem> items;
    std::vector<Light> lights;
};

/// @brief Vertex attribute layout for Mesh::setData.
struct VertexAttrib {
    gl::GLuint index;
    gl::GLint size;
    gl::GLenum type;
    bool normalized;
    gl::GLsizei stride;
    gl::GLintptr offset;
};

} // namespace fuhgl::render
