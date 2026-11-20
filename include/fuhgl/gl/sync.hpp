#pragma once

/// @file sync.hpp
/// @brief RAII wrappers for GL query objects (timer) and fence syncs.

#include "types.hpp"

#include <memory>

namespace fuhgl {

/// @brief GL query object, e.g. GL_TIME_ELAPSED for GPU timing.
class Query {
public:
    explicit Query(gl::GLenum target = gl::GL_TIME_ELAPSED);
    ~Query();

    Query(const Query&) = delete;
    Query& operator=(const Query&) = delete;
    Query(Query&&) noexcept;
    Query& operator=(Query&&) noexcept;

    gl::GLenum target() const;
    void begin();
    void end();
    /// @brief True when the result is available.
    bool resultAvailable() const;
    /// @brief Result in nanoseconds (GL_TIME_ELAPSED); blocks until available.
    gl::GLuint64 result() const;
    /// @brief Record a timestamp (GL_TIMESTAMP) — call outside begin/end.
    void recordTimestamp();

    gl::GLuint id() const;
    explicit operator bool() const { return id() != 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// @brief Fence sync object: insert into the GL command stream, wait on the client.
class Fence {
public:
    Fence(); // inserts the fence immediately
    ~Fence();

    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;
    Fence(Fence&&) noexcept;
    Fence& operator=(Fence&&) noexcept;

    /// @brief Wait up to timeoutNs nanoseconds. Returns the GL status.
    gl::GLenum wait(gl::GLuint64 timeoutNs = gl::GL_TIMEOUT_IGNORED) const;
    bool signaled() const { return wait(0) == gl::GL_ALREADY_SIGNALED || wait(0) == gl::GL_CONDITION_SATISFIED; }

    gl::GLsync handle() const;
    explicit operator bool() const { return handle() != nullptr; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl
