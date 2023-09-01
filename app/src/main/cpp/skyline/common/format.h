// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#pragma once

#include <bitset>
#include <fmt/format.h>

namespace fmt {
    /**
     * @brief A std::bitset formatter for {fmt}
     */
    template<size_t N>
    struct formatter<std::bitset<N>> : formatter<std::string> {
        template<typename FormatContext>
        constexpr auto format(const std::bitset<N> &s, FormatContext &ctx) {
            return formatter<std::string>::format(s.to_string(), ctx);
        }
    };

    template<class T> requires std::is_enum_v<T>
    struct formatter<T> : formatter<std::underlying_type_t<T>> {
        template<typename FormatContext>
        constexpr auto format(T s, FormatContext &ctx) {
            return formatter<std::underlying_type_t<T >>::format(static_cast<std::underlying_type_t<T>>(s), ctx);
        }
    };
}
