#pragma once

namespace skyline {
    namespace guest {
        constexpr size_t saveCtxSize = 20 * sizeof(u32); //!< The size of the SaveCtx function in 32-bit ARMv8 instructions
        constexpr size_t loadCtxSize = 20 * sizeof(u32); //!< The size of the LoadCtx function in 32-bit ARMv8 instructions
        constexpr size_t rescaleClockSize = 16 * sizeof(u32); //!< The size of the RescaleClock function in 32-bit ARMv8 instructions
        #ifdef NDEBUG
        constexpr size_t svcHandlerSize = 225 * sizeof(u32); //!< The size of the SvcHandler (Release) function in 32-bit ARMv8 instructions
        #else
        constexpr size_t svcHandlerSize = 400 * sizeof(u32); //!< The size of the SvcHandler (Debug) function in 32-bit ARMv8 instructions
        #endif

        /**
         * @brief This is the entry point for all guest threads
         * @param address The address of the actual thread entry point
         */
        void GuestEntry(u64 address);

        /**
         * @brief This saves the context from CPU registers into TLS
         */
        extern "C" void SaveCtx(void);

        /**
         * @brief This loads the context from TLS into CPU registers
         */
        extern "C" void LoadCtx(void);

        /**
         * @brief This rescales the clock to Tegra X1 levels and puts the output on stack
         */
        extern "C" __noreturn void RescaleClock(void);

        /**
         * @brief This is used to handle all SVC calls
         * @param pc The address of PC when the call was being done
         * @param svc The SVC ID of the SVC being called
         */
        void SvcHandler(u64 pc, u32 svc);
    }
}
