// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

/**
 * @brief A case statement for an enumerant value to use alongside ENUM_STRING
 */
#define ENUM_CASE(key)      \
    case ENUM_TYPE::key:    \
    return #key

/**
 * @brief Creates a function to convert an enumerant to its string representation at runtime
 * @example ENUM_STRING(Example, { ENUM_CASE(A); ENUM_CASE(B); })
 */
#define ENUM_STRING(name, cases)                        \
    constexpr const char *ToString(name value) {        \
        using ENUM_TYPE = name;                         \
        switch (value) {                                \
            cases                                       \
            default:                                    \
                return "Unknown";                       \
        };                                              \
    };
