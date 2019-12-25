#pragma once

namespace skyline::guest {
    constexpr size_t saveCtxSize = 20 * sizeof(u32);
    constexpr size_t loadCtxSize = 20 * sizeof(u32);
    extern "C" void saveCtx(void);
    extern "C" void loadCtx(void);
}
