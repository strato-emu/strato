#pragma once

namespace skyline {
    namespace guest {
        constexpr size_t saveCtxSize = 20 * sizeof(u32);
        constexpr size_t loadCtxSize = 20 * sizeof(u32);
        constexpr size_t svcHandlerSize = 200 * sizeof(u32);
        void entry(u64 address);
        extern "C" void saveCtx(void);
        extern "C" void loadCtx(void);
        void svcHandler(u64 pc, u32 svc);
    }
}
