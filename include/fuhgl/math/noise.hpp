#pragma once

/// @file noise.hpp
/// @brief Deterministic seeded gradient noise (improved Perlin 2D/3D) and fractal
/// Brownian motion (fBm). Pure value type, header-only — no GL, no allocation.
///
/// Values are roughly in [-1, 1] (raw improved-Perlin, not renormalized; the hard
/// bound is 2). Same seed -> same values, on every platform.

#include <cmath>
#include <cstdint>

namespace fuhgl::math {

namespace detail {

/// @brief splitmix32 PRNG step (used to build the seeded permutation).
inline uint32_t splitmix32(uint32_t& s) {
    s += 0x9E3779B9u;
    uint32_t z = s;
    z = (z ^ (z >> 16)) * 0x21F0AAADu;
    z = (z ^ (z >> 15)) * 0x735A2D97u;
    return z ^ (z >> 15);
}

/// @brief Perlin's quintic fade curve 6t^5 - 15t^4 + 10t^3.
inline float fade(float t) { return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }

/// @brief Perlin's grad(): dot of a pseudo-random gradient (hash h & 15) with (x, y, z).
inline float grad(uint8_t h, float x, float y, float z) {
    const int hh = h & 15;
    const float u = hh < 8 ? x : y;
    const float v = hh < 4 ? y : (hh == 12 || hh == 14 ? x : z);
    return ((hh & 1) ? -u : u) + ((hh & 2) ? -v : v);
}

} // namespace detail

/// @brief Seeded permutation-table noise source (value type, 512 bytes).
/// Copyable; construct once and reuse for many samples (cheaper than re-seeding).
class Noise {
public:
    /// @brief Builds the permutation table from `seed` (Fisher-Yates over splitmix32).
    explicit Noise(uint32_t seed = 0) {
        uint8_t base[256];
        for (int i = 0; i < 256; ++i) base[i] = static_cast<uint8_t>(i);
        uint32_t s = seed;
        for (int i = 255; i > 0; --i) {
            const int j = static_cast<int>(detail::splitmix32(s) % static_cast<uint32_t>(i + 1));
            const uint8_t t = base[i];
            base[i] = base[j];
            base[j] = t;
        }
        for (int i = 0; i < 512; ++i) p_[i] = base[i & 255];
    }

    /// @brief Improved Perlin noise, 2D. Integer lattice points evaluate to 0.
    float perlin2(float x, float y) const {
        const int xi = static_cast<int>(std::floor(x));
        const int yi = static_cast<int>(std::floor(y));
        const float fx = x - xi, fy = y - yi;
        const float u = detail::fade(fx), v = detail::fade(fy);
        const float aa = detail::grad(hash(xi, yi, 0), fx, fy, 0.f);
        const float ba = detail::grad(hash(xi + 1, yi, 0), fx - 1.f, fy, 0.f);
        const float ab = detail::grad(hash(xi, yi + 1, 0), fx, fy - 1.f, 0.f);
        const float bb = detail::grad(hash(xi + 1, yi + 1, 0), fx - 1.f, fy - 1.f, 0.f);
        const float x1 = aa + (ba - aa) * u;
        const float x2 = ab + (bb - ab) * u;
        return x1 + (x2 - x1) * v;
    }

    /// @brief Improved Perlin noise, 3D. Integer lattice points evaluate to 0.
    float perlin3(float x, float y, float z) const {
        const int xi = static_cast<int>(std::floor(x));
        const int yi = static_cast<int>(std::floor(y));
        const int zi = static_cast<int>(std::floor(z));
        const float fx = x - xi, fy = y - yi, fz = z - zi;
        const float u = detail::fade(fx), v = detail::fade(fy), w = detail::fade(fz);
        const float n000 = detail::grad(hash(xi, yi, zi), fx, fy, fz);
        const float n100 = detail::grad(hash(xi + 1, yi, zi), fx - 1.f, fy, fz);
        const float n010 = detail::grad(hash(xi, yi + 1, zi), fx, fy - 1.f, fz);
        const float n110 = detail::grad(hash(xi + 1, yi + 1, zi), fx - 1.f, fy - 1.f, fz);
        const float n001 = detail::grad(hash(xi, yi, zi + 1), fx, fy, fz - 1.f);
        const float n101 = detail::grad(hash(xi + 1, yi, zi + 1), fx - 1.f, fy, fz - 1.f);
        const float n011 = detail::grad(hash(xi, yi + 1, zi + 1), fx, fy - 1.f, fz - 1.f);
        const float n111 = detail::grad(hash(xi + 1, yi + 1, zi + 1), fx - 1.f, fy - 1.f, fz - 1.f);
        const float x00 = n000 + (n100 - n000) * u;
        const float x10 = n010 + (n110 - n010) * u;
        const float x01 = n001 + (n101 - n001) * u;
        const float x11 = n011 + (n111 - n011) * u;
        const float y0 = x00 + (x10 - x00) * v;
        const float y1 = x01 + (x11 - x01) * v;
        return y0 + (y1 - y0) * w;
    }

    /// @brief Fractal Brownian motion over perlin2, normalized by the total amplitude
    /// so the result stays in the same range as perlin2.
    float fbm2(float x, float y, int octaves, float lacunarity = 2.f, float gain = 0.5f) const {
        float sum = 0.f, amp = 1.f, freq = 1.f, norm = 0.f;
        for (int i = 0; i < octaves; ++i) {
            sum += amp * perlin2(x * freq, y * freq);
            norm += amp;
            amp *= gain;
            freq *= lacunarity;
        }
        return norm > 0.f ? sum / norm : 0.f;
    }

    /// @brief Fractal Brownian motion over perlin3, normalized by the total amplitude.
    float fbm3(float x, float y, float z, int octaves, float lacunarity = 2.f, float gain = 0.5f) const {
        float sum = 0.f, amp = 1.f, freq = 1.f, norm = 0.f;
        for (int i = 0; i < octaves; ++i) {
            sum += amp * perlin3(x * freq, y * freq, z * freq);
            norm += amp;
            amp *= gain;
            freq *= lacunarity;
        }
        return norm > 0.f ? sum / norm : 0.f;
    }

private:
    /// @brief Perlin's permutation hash: p[p[p[x] + y] + z].
    uint8_t hash(int x, int y, int z) const {
        return p_[(p_[(p_[x & 255] + y) & 255] + z) & 255];
    }

    uint8_t p_[512];
};

/// @brief One-shot 2D Perlin noise with a seed (constructs a Noise internally).
inline float perlin2(float x, float y, uint32_t seed = 0) { return Noise(seed).perlin2(x, y); }
/// @brief One-shot 3D Perlin noise with a seed (constructs a Noise internally).
inline float perlin3(float x, float y, float z, uint32_t seed = 0) { return Noise(seed).perlin3(x, y, z); }
/// @brief One-shot 2D fBm with a seed (constructs a Noise internally).
inline float fbm2(float x, float y, int octaves, uint32_t seed = 0, float lacunarity = 2.f, float gain = 0.5f) {
    return Noise(seed).fbm2(x, y, octaves, lacunarity, gain);
}
/// @brief One-shot 3D fBm with a seed (constructs a Noise internally).
inline float fbm3(float x, float y, float z, int octaves, uint32_t seed = 0, float lacunarity = 2.f, float gain = 0.5f) {
    return Noise(seed).fbm3(x, y, z, octaves, lacunarity, gain);
}

} // namespace fuhgl::math
