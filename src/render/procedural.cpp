#include "fuhgl/render/procedural.hpp"

#include "fuhgl/gl/loader.hpp"

#include <cmath>

namespace fuhgl::render::procedural {
namespace {

void fill(Image& img, int w, int h) {
    img.width = w;
    img.height = h;
    img.rgba.assign(static_cast<size_t>(w) * h * 4, 0);
}

void put(Image& img, int x, int y, const math::Vec3& c) {
    if (x < 0 || y < 0 || x >= img.width || y >= img.height) return;
    unsigned char* p = img.at(x, y);
    p[0] = static_cast<unsigned char>(std::fmin(1.f, std::fmax(0.f, c.x)) * 255.f + 0.5f);
    p[1] = static_cast<unsigned char>(std::fmin(1.f, std::fmax(0.f, c.y)) * 255.f + 0.5f);
    p[2] = static_cast<unsigned char>(std::fmin(1.f, std::fmax(0.f, c.z)) * 255.f + 0.5f);
    p[3] = 255;
}

// Deterministic integer hash -> [0,1)
float hash2(int x, int y, unsigned seed) {
    unsigned h = seed * 0x9E3779B1u + static_cast<unsigned>(x) * 0x85EBCA6Bu + static_cast<unsigned>(y) * 0xC2B2AE35u;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.f;
}

float smooth(float t) { return t * t * (3.f - 2.f * t); }

// Smooth value noise in [0,1]
float valueNoise(float x, float y, unsigned seed) {
    const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
    const float fx = x - xi, fy = y - yi;
    const float a = hash2(xi, yi, seed), b = hash2(xi + 1, yi, seed);
    const float c = hash2(xi, yi + 1, seed), d = hash2(xi + 1, yi + 1, seed);
    const float u = smooth(fx), v = smooth(fy);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}

float fbm(float x, float y, unsigned seed, int octaves) {
    float sum = 0.f, amp = 0.5f, freq = 1.f, norm = 0.f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * valueNoise(x * freq, y * freq, seed + static_cast<unsigned>(i) * 101u);
        norm += amp;
        amp *= 0.5f;
        freq *= 2.f;
    }
    return norm > 0.f ? sum / norm : 0.f;
}

} // namespace

Image checker(int w, int h, int cells, const math::Vec3& c1, const math::Vec3& c2) {
    Image img;
    fill(img, w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int cx = x * cells / w, cy = y * cells / h;
            put(img, x, y, ((cx + cy) % 2 == 0) ? c1 : c2);
        }
    return img;
}

Image gradient(int w, int h, const math::Vec3& bottom, const math::Vec3& top) {
    Image img;
    fill(img, w, h);
    for (int y = 0; y < h; ++y) {
        const float t = h > 1 ? static_cast<float>(y) / (h - 1) : 0.f;
        const math::Vec3 c = bottom + (top - bottom) * t;
        for (int x = 0; x < w; ++x) put(img, x, y, c);
    }
    return img;
}

Image stripes(int w, int h, int count, const math::Vec3& c1, const math::Vec3& c2, bool vertical) {
    Image img;
    fill(img, w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int i = vertical ? x * count / w : y * count / h;
            put(img, x, y, (i % 2 == 0) ? c1 : c2);
        }
    return img;
}

Image bricks(int w, int h, int rows, int cols, const math::Vec3& brick, const math::Vec3& mortar, float mortarFrac) {
    Image img;
    fill(img, w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float fy = static_cast<float>(y) / h * rows;
            const int row = static_cast<int>(fy);
            const float inRow = fy - row;
            const float fx = (static_cast<float>(x) / w * cols + (row % 2 ? 0.5f : 0.f));
            const int col = static_cast<int>(fx);
            const float inCol = fx - col;
            const bool isMortar = inRow < mortarFrac || inCol < mortarFrac;
            put(img, x, y, isMortar ? mortar : brick);
        }
    return img;
}

Image rings(int w, int h, int count, const math::Vec3& c1, const math::Vec3& c2) {
    Image img;
    fill(img, w, h);
    const float cx = (w - 1) * 0.5f, cy = (h - 1) * 0.5f;
    const float maxR = std::sqrt(cx * cx + cy * cy);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float dx = x - cx, dy = y - cy;
            const float r = std::sqrt(dx * dx + dy * dy) / maxR * count;
            put(img, x, y, (static_cast<int>(r) % 2 == 0) ? c1 : c2);
        }
    return img;
}

Image noise(int w, int h, unsigned seed, int octaves, const math::Vec3& c1, const math::Vec3& c2) {
    Image img;
    fill(img, w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float n = fbm(static_cast<float>(x) / w * 6.f, static_cast<float>(y) / h * 6.f, seed, octaves);
            put(img, x, y, c1 + (c2 - c1) * n);
        }
    return img;
}

Image heightmap(int w, int h, unsigned seed, int octaves, float scale) {
    Image img;
    fill(img, w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float v = fbm(static_cast<float>(x) * scale, static_cast<float>(y) * scale, seed, octaves);
            put(img, x, y, {v, v, v});
        }
    return img;
}

Image voronoi(int w, int h, int cells, unsigned seed, const math::Vec3& c1, const math::Vec3& c2) {
    Image img;
    fill(img, w, h);
    const float cw = static_cast<float>(w) / cells, chh = static_cast<float>(h) / cells;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float px = static_cast<float>(x), py = static_cast<float>(y);
            const int cx = static_cast<int>(px / cw), cy = static_cast<int>(py / chh);
            float d1 = 1e9f, d2 = 1e9f;
            int best = -1;
            for (int oy = -1; oy <= 1; ++oy)
                for (int ox = -1; ox <= 1; ++ox) {
                    const int gx = cx + ox, gy = cy + oy;
                    if (gx < 0 || gy < 0 || gx >= cells || gy >= cells) continue;
                    const float jx = gx * cw + cw * 0.5f + (hash2(gx, gy, seed) - 0.5f) * cw * 0.8f;
                    const float jy = gy * chh + chh * 0.5f + (hash2(gx, gy, seed + 7u) - 0.5f) * chh * 0.8f;
                    const float d = std::sqrt((px - jx) * (px - jx) + (py - jy) * (py - jy));
                    if (d < d1) { d2 = d1; d1 = d; best = gx + gy * cells; }
                    else if (d < d2) { d2 = d; }
                }
            const float edge = std::fmin(1.f, (d2 - d1) / (cw * 0.25f)); // dark cell borders
            const float shade = hash2(best, 0, seed + 13u);
            const math::Vec3 cellCol = c1 + (c2 - c1) * shade;
            put(img, x, y, cellCol * (0.25f + 0.75f * edge));
        }
    return img;
}

Texture makeTexture(const Image& img, bool mipmaps) {
    Texture tex(Texture::Target::Tex2D);
    tex.bind();
    const bool canMipmap = mipmaps && gl::functions().glTexStorage2D && gl::functions().glGenerateMipmap;
    if (canMipmap) {
        gl::GLsizei levels = 1;
        for (int s = std::max(img.width, img.height); s > 1; s /= 2) ++levels;
        tex.texStorage2D(levels, gl::GL_RGBA8, img.width, img.height); // immutable storage, all levels
        tex.setSubImage2D(0, 0, 0, img.width, img.height, gl::GL_RGBA, gl::GL_UNSIGNED_BYTE, img.rgba.data());
        tex.generateMipmap();
        tex.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR_MIPMAP_LINEAR);
    } else {
        tex.setImage2D(0, gl::GL_RGBA8, img.width, img.height, gl::GL_RGBA, gl::GL_UNSIGNED_BYTE, img.rgba.data());
        tex.setParameteri(gl::GL_TEXTURE_MIN_FILTER, gl::GL_LINEAR);
    }
    tex.setParameteri(gl::GL_TEXTURE_MAG_FILTER, gl::GL_LINEAR);
    tex.setParameteri(gl::GL_TEXTURE_WRAP_S, gl::GL_REPEAT);
    tex.setParameteri(gl::GL_TEXTURE_WRAP_T, gl::GL_REPEAT);
    return tex;
}

Image normalFromHeight(const Image& height, float strength) {
    Image out;
    out.width = height.width;
    out.height = height.height;
    out.rgba.resize(height.rgba.size());
    auto hAt = [&](int x, int y) {
        x = (x % height.width + height.width) % height.width;
        y = (y % height.height + height.height) % height.height;
        return height.at(x, y)[0] / 255.f;
    };
    for (int y = 0; y < height.height; ++y)
        for (int x = 0; x < height.width; ++x) {
            const float dx = (hAt(x + 1, y) - hAt(x - 1, y)) * strength;
            const float dy = (hAt(x, y + 1) - hAt(x, y - 1)) * strength;
            // normal = normalize(-dx, -dy, 1) in tangent space (u = +x, v = +y)
            const float inv = 1.f / std::sqrt(dx * dx + dy * dy + 1.f);
            unsigned char* o = out.at(x, y);
            o[0] = static_cast<unsigned char>((dx * -inv * 0.5f + 0.5f) * 255.f);
            o[1] = static_cast<unsigned char>((dy * -inv * 0.5f + 0.5f) * 255.f);
            o[2] = static_cast<unsigned char>((inv * 0.5f + 0.5f) * 255.f);
            o[3] = 255;
        }
    return out;
}

Texture makeNormalMap(const Image& height, float strength) {
    return makeTexture(normalFromHeight(height, strength));
}

} // namespace fuhgl::render::procedural
