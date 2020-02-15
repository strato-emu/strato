#pragma once

namespace skyline {
    namespace guest {
        constexpr size_t saveCtxSize = 20 * sizeof(u32);
        constexpr size_t loadCtxSize = 20 * sizeof(u32);
        constexpr size_t rescaleClockSize = 16 * sizeof(u32);
        #ifdef NDEBUG
        constexpr size_t svcHandlerSize = 225 * sizeof(u32);
        #else
        constexpr size_t svcHandlerSize = 400 * sizeof(u32);
        #endif

        void GuestEntry(u64 address);

        extern "C" void SaveCtx(void);
        extern "C" void LoadCtx(void);
        extern "C" __noreturn void RescaleClock(void);

        void SvcHandler(u64 pc, u32 svc);
    }
}
