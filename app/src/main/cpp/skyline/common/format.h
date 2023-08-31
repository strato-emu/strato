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

namespace skyline::format {
    /**
     * @brief A way to apply a transformation to a paramater pack.
     * @tparam Out The output type
     * @tparam Fun The transformation to apply to the parameter pack
     */
    template<template<typename...> class Out,
        template<typename> class Fun,
        typename... Args>
    struct TransformArgs {
        using Type = Out<typename Fun<Args>::Type...>;
    };

    /**
     * @brief Casts all pointer types to uintptr_t, this is used for {fmt} as we use 0x{:X} to print pointers
     * @note There's the exception of signed char pointers as they represent C Strings
     */
    template<typename T>
    struct PointerCast {
      private:
        using NakedT = std::remove_cvref_t<T>; // Remove const, volatile, and reference qualifiers
      public:
        using Type =
            std::conditional_t<std::is_pointer_v<NakedT>, // If the type is a pointer
                               std::conditional_t<std::is_same_v<char, std::remove_pointer_t<NakedT>>, // If the type is a char pointer
                                                  char *, // Cast to char *
                                                  uintptr_t>, // Otherwise cast to uintptr_t
                               T>; // The type isn't a pointer so just return it
    };

    /**
     * @brief A way to implicitly cast all pointers to uintptr_t, this is used for {fmt} as we use 0x{:X} to print pointers
     * @note There's the exception of signed char pointers as they represent C Strings
     * @note This does not cover std::shared_ptr or std::unique_ptr and those will have to be explicitly casted to uintptr_t or passed through fmt::ptr
     */
    template<typename T>
    constexpr auto FmtCast(T object) {
        if constexpr (std::is_pointer_v<T>)
            if constexpr (std::is_same_v<char, typename std::remove_cv_t<typename std::remove_pointer_t<T>>>)
                return reinterpret_cast<typename std::common_type_t<char *, T>>(object);
            else
                return reinterpret_cast<const uintptr_t>(object);
        else
            return object;
    }

    /**
     * @brief A wrapper around fmt::format_string which casts all pointer types to uintptr_t
     */
    template<typename... Args>
    using FormatString = typename TransformArgs<fmt::format_string, PointerCast, Args...>::Type;

    /**
     * @brief {fmt}::format but with FmtCast built into it
     */
    template<typename... Args>
    constexpr auto Format(FormatString<Args...> formatString, Args &&... args) {
        return fmt::vformat(formatString, fmt::make_format_args(FmtCast(args)...));
    }
}
