#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/render/text.hpp>

using namespace fuhgl::render;

void runTextTests() {
    // embedded font: 128 glyphs, 8 bytes each; space and NUL are blank, 'A' is not
    CHECK(kFontGlyphCount == 128);
    CHECK(kFontGlyphSize == 8);
    bool aNonZero = false, spaceZero = true, nulZero = true;
    for (int i = 0; i < 8; ++i) {
        if (font8x8Glyph('A')[i] != 0) aNonZero = true;
        if (font8x8Glyph(' ')[i] != 0) spaceZero = false;
        if (font8x8Glyph(0)[i] != 0) nulZero = false;
    }
    CHECK(aNonZero);
    CHECK(spaceZero);
    CHECK(nulZero);
    // known glyph shape (font8x8 basic latin): '!' stem rows 0..3 and dot at row 6
    CHECK(font8x8Glyph('!')[0] == 0x18 && font8x8Glyph('!')[5] == 0x00 && font8x8Glyph('!')[6] == 0x18);
    // chars outside ASCII are masked into the table (never out of bounds)
    CHECK(font8x8Glyph(static_cast<unsigned char>(0xC3)) == font8x8Glyph('C'));

    // measure: one 8px cell per char, height = one cell
    CHECK_NEAR(TextRenderer::measure("", 1.f).x, 0.f, 1e-6f);
    CHECK_NEAR(TextRenderer::measure("AB", 2.f).x, 32.f, 1e-6f);
    CHECK_NEAR(TextRenderer::measure("AB", 2.f).y, 16.f, 1e-6f);
    CHECK_NEAR(TextRenderer::lineHeight(1.5f), 12.f, 1e-6f);

    // FpsCounter: fps is 0 before two frames, then ~1/dt with EMA smoothing
    FpsCounter fc;
    CHECK(fc.fps() == 0.f);
    fc.frame(1.0);
    CHECK(fc.fps() == 0.f); // only one frame so far
    fc.frame(1.5);
    CHECK_NEAR(fc.fps(), 2.f, 1e-4f);
    fc.frame(2.0);
    CHECK_NEAR(fc.fps(), 2.f, 1e-4f); // EMA of 2 and 2 stays 2
    fc.frame(2.25);                   // instant 4 fps -> ema = 2*0.9 + 4*0.1 = 2.2
    CHECK_NEAR(fc.fps(), 2.2f, 1e-4f);
    CHECK(fc.frames == 4);
    fc.reset();
    CHECK(fc.fps() == 0.f && fc.frames == 0);
}
