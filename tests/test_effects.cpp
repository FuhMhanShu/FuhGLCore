// Unit tests for F39/F40: render::effectSupported mapping (pure function, no GL),
// effectName, and the Effect enum size.

#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/render/effects.hpp>

using namespace fuhgl;

void runEffectsTests() {
    using render::Effect;
    using render::effectSupported;

    // GL 2.1: nothing GPU-side
    CHECK(!effectSupported(Effect::Shadows, 2, 1, false, false));
    CHECK(!effectSupported(Effect::DoF, 2, 1, false, false));
    CHECK(!effectSupported(Effect::CsmShadows, 3, 2, false, false)); // needs 3.3 shaders
    // GL 3.3: all shader-based effects on
    for (Effect e : {Effect::Shadows, Effect::CsmShadows, Effect::Ssao, Effect::Ibl, Effect::Bloom,
                     Effect::GodRays, Effect::Taa, Effect::Ssr, Effect::DoF, Effect::VolFog})
        CHECK(effectSupported(e, 3, 3, false, false));
    CHECK(effectSupported(Effect::VolFog, 4, 6, false, false));
    // compute/culling: only with 4.3+ or the ARB extension flag
    CHECK(!effectSupported(Effect::GpuCulling, 3, 3, false, false));
    CHECK(effectSupported(Effect::GpuCulling, 3, 3, false, true));
    CHECK(effectSupported(Effect::GpuCulling, 4, 3, false, false));
    // bindless: only with the extension
    CHECK(!effectSupported(Effect::BindlessTextures, 4, 6, false, false));
    CHECK(effectSupported(Effect::BindlessTextures, 4, 6, true, false));
    // CPU-side features: always
    CHECK(effectSupported(Effect::Lod, 2, 1, false, false));
    CHECK(effectSupported(Effect::HeightmapTerrain, 0, 0, false, false));
    // names: all effects have a non-empty name, enum count matches
    for (int i = 0; i < render::kEffectCount; ++i) {
        const char* n = render::effectName(static_cast<Effect>(i));
        CHECK(n != nullptr && n[0] != '\0');
    }
    // F46: point shadows + contact shadows are GLSL 330 features (GL >= 3.3)
    CHECK(!effectSupported(Effect::PointShadows, 3, 2, false, false));
    CHECK(effectSupported(Effect::PointShadows, 3, 3, false, false));
    CHECK(effectSupported(Effect::ContactShadows, 4, 0, false, false));
    CHECK(render::kEffectCount == 16);
}
