#pragma once

/// @file ubo.hpp
/// @brief Uniform buffer wrapper: typed upload + indexed binding point.

#include "buffer.hpp"

#include <cstring>

namespace fuhgl {

/// @brief Uniform buffer object (GL_UNIFORM_BUFFER) with a typed upload helper.
/// Usage: create(), upload(myStruct), bindBase(bindingIndex).
class UniformBuffer {
public:
    UniformBuffer() = default;

    bool create() { return buffer_.create(Buffer::Target::Uniform); }
    void bind() const { buffer_.bind(); }

    /// @brief Allocates the store and uploads data (bind first or use upload after create).
    template <typename T>
    void upload(const T& data, Buffer::Usage usage = Buffer::Usage::DynamicDraw) {
        buffer_.bind();
        buffer_.setData(sizeof(T), &data, usage);
    }
    /// @brief Updates a sub-region (e.g. just the viewProj matrix inside a camera block).
    template <typename T>
    void uploadSub(gl::GLintptr offset, const T& data) {
        buffer_.bind();
        buffer_.setSubData(offset, sizeof(T), &data);
    }
    /// @brief Binds to an indexed binding point so shaders can use layout(binding=N).
    void bindBase(gl::GLuint index) const { buffer_.bindBase(index); }

    Buffer& buffer() { return buffer_; }
    const Buffer& buffer() const { return buffer_; }
    gl::GLuint id() const { return buffer_.id(); }
    explicit operator bool() const { return buffer_.id() != 0; }

private:
    Buffer buffer_;
};

} // namespace fuhgl
