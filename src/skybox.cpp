#include "fuhgl/skybox.hpp"
#include "fuhgl/gl/loader.hpp"
#include "fuhgl/gl/state.hpp"

#include <vector>

namespace fuhgl {
namespace {

constexpr const char* kSkyVertSrc = R"GLSL(#version 330 core
uniform mat4 uInvViewProj; // inverse of proj * view (translation stripped)
out vec3 vDir;
void main() {
    // fullscreen triangle from gl_VertexID (no VBO needed)
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;
    gl_Position = vec4(pos, 0.999999, 1.0); // just inside the far plane
    vec4 farP = uInvViewProj * vec4(pos, 1.0, 1.0);
    vec4 nearP = uInvViewProj * vec4(pos, -1.0, 1.0);
    vDir = normalize(farP.xyz / farP.w - nearP.xyz / nearP.w);
}
)GLSL";

constexpr const char* kSkyFragSrc = R"GLSL(#version 330 core
in vec3 vDir;
uniform samplerCube uSky;
out vec4 FragColor;
void main() { FragColor = texture(uSky, vDir); }
)GLSL";

// Skybox geometry is a fullscreen triangle generated from gl_VertexID in the shader;
// the VAO only needs to exist (core profile requires one bound).

} // namespace

struct Skybox::Impl {
    Shader vs{Shader::Type::Vertex};
    Shader fs{Shader::Type::Fragment};
    Program program;
    VertexArray vao;
    bool ready = false;
};

Skybox::Skybox() : impl_(std::make_unique<Impl>()) {}
Skybox::~Skybox() = default;
Skybox::Skybox(Skybox&&) noexcept = default;
Skybox& Skybox::operator=(Skybox&&) noexcept = default;

bool Skybox::create() {
    if (!texture_.id()) texture_.create(Texture::Target::Cube);
    if (!impl_->vs.compile(kSkyVertSrc)) return false;
    if (!impl_->fs.compile(kSkyFragSrc)) return false;
    if (!impl_->program.create()) return false;
    impl_->program.attach(impl_->vs);
    impl_->program.attach(impl_->fs);
    if (!impl_->program.link()) return false;

    impl_->vao.create();
    impl_->vao.bind(); // empty VAO: geometry comes from gl_VertexID

    impl_->ready = true;
    return true;
}

bool Skybox::loadFaces(const void* const faces[6], gl::GLsizei size, gl::GLenum internalFormat,
                       gl::GLenum format, gl::GLenum type) {
    if (!impl_->ready) return false;
    texture_.bind();
    const Texture::CubeFace order[6] = {
        Texture::CubeFace::PosX, Texture::CubeFace::NegX, Texture::CubeFace::PosY,
        Texture::CubeFace::NegY, Texture::CubeFace::PosZ, Texture::CubeFace::NegZ,
    };
    for (int i = 0; i < 6; ++i)
        texture_.setImageCubeFace(order[i], 0, internalFormat, size, format, type, faces[i]);
    texture_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR);
    texture_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
    texture_.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    texture_.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    texture_.setParameteri(gl::GL_TEXTURE_WRAP_R, gl::GL_CLAMP_TO_EDGE);
    return true;
}

void Skybox::makeGradient(const math::Vec3& topColor, const math::Vec3& horizonColor, gl::GLsizei size) {
    if (!impl_->ready) return;
    texture_.bind();
    std::vector<unsigned char> img(static_cast<size_t>(size) * size * 3);
    for (gl::GLsizei y = 0; y < size; ++y) {
        const float t = static_cast<float>(y) / static_cast<float>(size - 1);
        const math::Vec3 c = math::lerp(horizonColor, topColor, t);
        for (gl::GLsizei x = 0; x < size; ++x) {
            const size_t o = (static_cast<size_t>(y) * size + x) * 3;
            img[o + 0] = static_cast<unsigned char>(math::clamp(c.x, 0.f, 1.f) * 255.f);
            img[o + 1] = static_cast<unsigned char>(math::clamp(c.y, 0.f, 1.f) * 255.f);
            img[o + 2] = static_cast<unsigned char>(math::clamp(c.z, 0.f, 1.f) * 255.f);
        }
    }
    const Texture::CubeFace order[6] = {
        Texture::CubeFace::PosX, Texture::CubeFace::NegX, Texture::CubeFace::PosY,
        Texture::CubeFace::NegY, Texture::CubeFace::PosZ, Texture::CubeFace::NegZ,
    };
    for (int i = 0; i < 6; ++i)
        texture_.setImageCubeFace(order[i], 0, gl::GL_RGB8, size, gl::GL_RGB, gl::GL_UNSIGNED_BYTE, img.data());
    texture_.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR);
    texture_.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
    texture_.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
    texture_.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
    texture_.setParameteri(gl::GL_TEXTURE_WRAP_R, gl::GL_CLAMP_TO_EDGE);
}

void Skybox::draw(const Camera& camera, float aspect) const {
    if (!impl_->ready) return;
    auto& f = gl::functions();
    f.glDepthFunc(gl::GL_LEQUAL);
    f.glDepthMask(gl::GL_FALSE);

    impl_->program.use();
    math::Mat4 view = camera.view();
    view(3, 0) = 0.f; view(3, 1) = 0.f; view(3, 2) = 0.f; // strip translation: skybox is infinitely far
    const math::Mat4 vp = camera.projection(aspect) * view;
    impl_->program.setUniform("uInvViewProj", vp.inverse());

    gl::activeTexture(gl::GL_TEXTURE0);
    f.glBindTexture(gl::GL_TEXTURE_CUBE_MAP, texture_.id());
    impl_->program.setUniform(impl_->program.uniformLocation("uSky"), 0);

    impl_->vao.bind();
    gl::drawArrays(gl::Primitive::Triangles, 0, 3); // fullscreen triangle

    f.glDepthMask(gl::GL_TRUE);
    f.glDepthFunc(gl::GL_LESS);
}

} // namespace fuhgl
