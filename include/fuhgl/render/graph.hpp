#pragma once

/// @file graph.hpp
/// @brief RenderGraph (F31): a small declarative render-pass graph. Declare named
/// texture resources and passes (inputs = sampled resources, outputs = render targets,
/// plus a callback); compile() resolves the execution order (topological sort with
/// cycle detection) and creates one FBO per pass; execute() runs the enabled passes
/// in order. A pass may declare `feedback = true` to also read its own output
/// (previous frame) — used for temporal accumulation (TAA, F32); the previous
/// content is copied into a hidden scratch texture before the pass runs, so a
/// pass never samples the texture it renders into (undefined behavior in GL).
///
/// The pure dependency resolution `resolvePassOrder()` is header-only and unit-testable
/// without a GL context.

#include "../gl/framebuffer.hpp"
#include "../gl/texture.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fuhgl::render {

/// @brief Pass declaration for the pure order resolver (no GL).
struct GraphPassDef {
    std::string name;
    std::vector<std::string> inputs;   ///< sampled resources
    std::vector<std::string> outputs;  ///< render-target resources
    bool feedback = false;             ///< may also read its own outputs (previous frame)
};

/// @brief Resolves the execution order of the passes: an edge A -> B exists when B
/// reads a resource that A writes (a resource may have several writers — ping-pong
/// targets; they are ordered by declaration). Feedback self-edges are ignored.
/// @return false on a dependency cycle. `orderOut` receives pass indices in execution order.
inline bool resolvePassOrder(const std::vector<GraphPassDef>& passes, std::vector<int>& orderOut) {
    const std::size_t n = passes.size();
    orderOut.clear();
    std::vector<std::vector<int>> adj(n);
    std::vector<int> indeg(n, 0);
    auto writersOf = [&](const std::string& res) {
        std::vector<int> w;
        for (std::size_t i = 0; i < n; ++i)
            for (const std::string& o : passes[i].outputs)
                if (o == res) w.push_back(static_cast<int>(i));
        return w;
    };
    for (std::size_t i = 0; i < n; ++i) {
        for (const std::string& in : passes[i].inputs) {
            const std::vector<int> ws = writersOf(in);
            int latestBefore = -1;
            for (int w : ws)
                if (w < static_cast<int>(i) && w != (passes[i].feedback ? static_cast<int>(i) : -2) && w > latestBefore)
                    latestBefore = w;
            if (latestBefore >= 0) {
                // ping-pong safe: depend only on the latest writer declared before this pass
                adj[latestBefore].push_back(static_cast<int>(i));
                ++indeg[i];
            } else {
                // no earlier writer: forward references — all of them become edges
                // (this is what makes real cycles detectable)
                for (int w : ws) {
                    if (w == static_cast<int>(i) && passes[i].feedback) continue; // previous frame
                    adj[w].push_back(static_cast<int>(i));
                    ++indeg[i];
                }
            }
        }
    }
    // Kahn's algorithm
    std::vector<int> queue;
    for (std::size_t i = 0; i < n; ++i)
        if (indeg[i] == 0) queue.push_back(static_cast<int>(i));
    while (!queue.empty()) {
        const int p = queue.back();
        queue.pop_back();
        orderOut.push_back(p);
        for (int nxt : adj[p])
            if (--indeg[nxt] == 0) queue.push_back(nxt);
    }
    return orderOut.size() == n; // false when a cycle remains
}

/// @brief Render graph (PIMPL, movable, non-copyable). Declare resources + passes,
/// compile(), then execute() every frame.
class RenderGraph {
public:
    /// @brief Per-pass context handed to the pass callback.
    struct PassContext {
        RenderGraph& graph;
        int pass = 0;             ///< index of the running pass (in declaration order)
        int width = 0;            ///< output size of this pass
        int height = 0;
        /// @brief Looks up a resource texture (owned or external), nullptr when unknown.
        const Texture* texture(std::string_view name) const;
        /// @brief Binds a resource to a texture unit (activeTexture + bind).
        void bindInput(int unit, std::string_view name) const;
        /// @brief The pass's own framebuffer (outputs attached in order).
        Framebuffer& framebuffer();
    };

    using PassFn = std::function<void(PassContext&)>;

    RenderGraph();
    ~RenderGraph();

    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) noexcept;
    RenderGraph& operator=(RenderGraph&&) noexcept;

    /// @brief Declares an owned texture resource (allocated at compile/resize).
    int addResource(std::string name, int width, int height, gl::GLenum internalFormat);
    /// @brief Declares an external (borrowed) texture resource, e.g. a pipeline G-buffer.
    int addExternal(std::string name, const Texture* tex, int width, int height);
    /// @brief Declares a pass. `feedback` allows reading its own outputs (previous frame).
    int addPass(std::string name, std::vector<std::string> inputs, std::vector<std::string> outputs,
                PassFn fn, bool feedback = false);
    /// @brief Enables/disables a pass at runtime (disabled passes are skipped in execute()).
    void setPassEnabled(std::string_view name, bool enabled);

    /// @brief Resolves the order and creates the per-pass FBOs. false on cycle/bad refs.
    bool compile();
    /// @brief Runs all enabled passes in the compiled order.
    void execute();
    /// @brief Reallocates owned resources and FBOs at a new size (passes stay).
    void resize(int width, int height);

    const Texture* texture(std::string_view name) const;
    int passCount() const { return passCount_; }
    /// @brief Compiled execution order (pass indices), valid after compile().
    const std::vector<int>& order() const { return order_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    int passCount_ = 0;
    std::vector<int> order_;
};

} // namespace fuhgl::render
