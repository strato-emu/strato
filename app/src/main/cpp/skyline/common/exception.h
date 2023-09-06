// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vector>
#include "format.h"

namespace skyline {
    /**
     * @brief A wrapper over std::runtime_error with {fmt} formatting and stack tracing support
     */
    class exception : public std::runtime_error {
      public:
        std::vector<void *> frames; //!< All frames from the stack trace during the exception

        /**
         * @return A vector of all frames on the caller's stack
         * @note This cannot be inlined because it depends on having one stack frame itself consistently
         */
        static std::vector<void *> GetStackFrames() __attribute__((noinline));

        template<typename... Args>
        exception(fmt::format_string<Args...> formatStr, Args &&... args) : runtime_error(fmt::format(formatStr, std::forward<Args>(args)...)), frames(GetStackFrames()) {}
    };
}
