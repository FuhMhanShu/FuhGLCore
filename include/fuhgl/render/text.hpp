#pragma once

/// @file text.hpp
/// @brief TextRenderer: screen-space bitmap text with an embedded 8x8 font atlas
/// (no external font files), plus FpsCounter for HUD/FPS overlays.

#include "../gl/buffer.hpp"
#include "../gl/program.hpp"
#include "../gl/shader.hpp"
#include "../gl/texture.hpp"
#include "../gl/vao.hpp"
#include "../math.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace fuhgl::render {

// --- embedded font (pure data, unit-testable without GL) ------------------------

/// @brief Number of glyphs in the embedded font (ASCII 0x00..0x7F).
constexpr int kFontGlyphCount = 128;
/// @brief Glyph cell edge length in pixels (square cells).
constexpr int kFontGlyphSize = 8;

/// @brief 8 row-bitmasks of glyph c (bit i = pixel column i, row 0 = top).
/// Public-domain 8x8 font (font8x8 by Daniel Hepper, based on the public-domain
/// IBM VGA font by Marcel Sondaar). Chars outside ASCII map to '?'.
const std::uint8_t* font8x8Glyph(unsigned char c);

// --- FPS counter (value type) ------------------------------------------------------

/// @brief Exponential-moving-average FPS counter; call frame() once per rendered frame.
struct FpsCounter {
    double lastTime = -1.0; ///< timestamp of the previous frame() call
    float ema = 0.f;        ///< smoothed fps (0 until the second frame)
    long frames = 0;        ///< total frame() calls

    /// @brief Records one frame at the given timestamp (seconds).
    void frame(double nowSeconds) {
        if (lastTime >= 0.0) {
            const double dt = nowSeconds - lastTime;
            if (dt > 0.0) {
                const float inst = static_cast<float>(1.0 / dt);
                ema = ema <= 0.f ? inst : ema * 0.9f + inst * 0.1f;
            }
        }
        lastTime = nowSeconds;
        ++frames;
    }
    /// @brief Smoothed frames per second (0 until at least two frames were recorded).
    float fps() const { return ema; }
    void reset() {
        lastTime = -1.0;
        ema = 0.f;
        frames = 0;
    }
};

// --- text renderer (PIMPL) ---------------------------------------------------------

/// @brief Screen-space text renderer with the embedded 8x8 bitmap font atlas.
///
/// Usage: create() once after a GL context exists; per frame call begin(w, h),
/// then draw(...) any number of times, then end(). Text is drawn into the
/// currently bound framebuffer with alpha blending, depth test off.
/// Coordinates are pixels with (0,0) = top-left and y growing downward.
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
    TextRenderer(TextRenderer&&) noexcept;
    TextRenderer& operator=(TextRenderer&&) noexcept;

    /// @brief Creates the font atlas texture, shader program and vertex buffers.
    bool create();
    /// @brief True after a successful create().
    bool isCreated() const;

    /// @brief Sets viewport + ortho projection + blending for a w x h framebuffer.
    void begin(int width, int height);
    /// @brief Flushes all queued glyphs (one draw call) and restores GL state.
    void end();

    /// @brief Queues one line of text; (x, y) = top-left corner in pixels (y down).
    /// @param scale glyph scale in pixels (1.0 = 8 px cells); @param color RGBA.
    void draw(const std::string& text, float x, float y, float scale, const math::Vec4& color);
    void draw(const std::string& text, float x, float y, float scale, const math::Vec3& color) {
        draw(text, x, y, scale, math::Vec4{color.x, color.y, color.z, 1.f});
    }

    /// @brief Size of the text in pixels at the given scale (one cell per char).
    static math::Vec2 measure(const std::string& text, float scale);
    /// @brief Line height in pixels at the given scale.
    static float lineHeight(float scale) { return static_cast<float>(kFontGlyphSize) * scale; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl::render
