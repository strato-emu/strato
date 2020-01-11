#pragma once

namespace skyline {
    namespace guest {
        constexpr size_t saveCtxSize = 20 * sizeof(u32);
        constexpr size_t loadCtxSize = 20 * sizeof(u32);
        #ifdef NDEBUG
        constexpr size_t svcHandlerSize = 150 * sizeof(u32);
        #else
        constexpr size_t svcHandlerSize = 250 * sizeof(u32);
        #endif

        void entry(u64 address);

        extern "C" void saveCtx(void);
        extern "C" void loadCtx(void);

        void svcHandler(u64 pc, u32 svc);
    }
}
