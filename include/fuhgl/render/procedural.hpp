#pragma once

/// @file procedural.hpp
/// @brief Procedural textures: CPU-side generators producing RGBA8 images
/// (deterministic, seeded) plus a helper that uploads them into a gl::Texture.
/// No dependencies — value noise / voronoi are implemented here.

#include "../gl/texture.hpp"
#include "../math.hpp"

#include <vector>

namespace fuhgl::render::procedural {

/// @brief CPU-side RGBA8 image.
struct Image {
    int width = 0, height = 0;
    std::vector<unsigned char> rgba; ///< width*height*4 bytes
    unsigned char* at(int x, int y) { return rgba.data() + (static_cast<size_t>(y) * width + x) * 4; }
    const unsigned char* at(int x, int y) const { return rgba.data() + (static_cast<size_t>(y) * width + x) * 4; }
};

Image checker(int w, int h, int cells, const math::Vec3& c1, const math::Vec3& c2);
Image gradient(int w, int h, const math::Vec3& bottom, const math::Vec3& top);
Image stripes(int w, int h, int count, const math::Vec3& c1, const math::Vec3& c2, bool vertical = true);
Image bricks(int w, int h, int rows, int cols, const math::Vec3& brick, const math::Vec3& mortar, float mortarFrac = 0.08f);
Image rings(int w, int h, int count, const math::Vec3& c1, const math::Vec3& c2);
/// @brief Seeded value-noise fBm (deterministic: same seed → same image).
Image noise(int w, int h, unsigned seed, int octaves = 4,
            const math::Vec3& c1 = {0.f, 0.f, 0.f}, const math::Vec3& c2 = {1.f, 1.f, 1.f});
/// @brief Seeded cellular (voronoi) pattern, edge-darkened (F2-F1 style).
Image voronoi(int w, int h, int cells, unsigned seed,
              const math::Vec3& c1 = {0.1f, 0.1f, 0.15f}, const math::Vec3& c2 = {0.9f, 0.85f, 0.7f});

/// @brief Uploads an Image into a 2D RGBA8 texture (with mipmaps + linear filtering by default).
/// Requires a current GL context (call after Window::create).
Texture makeTexture(const Image& img, bool mipmaps = true);

/// @brief Grayscale heightmap image (F38): seeded fBm noise mapped to [0, 1] in the red
/// channel (green/blue identical, alpha 255) — ready source for terrain heightmaps.
/// Deterministic per seed. `scale` = noise frequency in pixels.
Image heightmap(int w, int h, unsigned seed, int octaves = 5, float scale = 0.08f);

/// @brief Derives a tangent-space normal map (RGBA8, rgb = normal*0.5+0.5) from a grayscale
/// height Image (uses its red channel). `strength` scales the height gradient. CPU-side, deterministic.
Image normalFromHeight(const Image& height, float strength = 1.f);

/// @brief Convenience: normal map as a ready-to-bind texture (mipmapped, linear).
Texture makeNormalMap(const Image& height, float strength = 1.f);

} // namespace fuhgl::render::procedural
