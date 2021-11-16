// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

/**
 * @brief A case statement for an enumerant value to use alongside ENUM_STRING
 */
#define ENUM_CASE(key)   \
    case ENUM_TYPE::key: \
    return #key

/**
 * @brief Creates a function to convert an enumerant to its string representation at runtime
 * @example ENUM_STRING(Example, { ENUM_CASE(A); ENUM_CASE(B); })
 */
#define ENUM_STRING(name, cases)                        \
    constexpr static const char *ToString(name value) { \
        using ENUM_TYPE = name;                         \
        switch (value) {                                \
            cases                                       \
            default:                                    \
                return "Unknown";                       \
        }                                               \
    }

/**
 * @brief A case statement for an enumerant value to use alongside ENUM_SWITCH
 */
#define ENUM_CASE_PAIR(key, value) \
    case ENUM_TYPE::key:           \
        return value

/**
 * @brief Creates a switch case statement to convert an enumerant to the given values
 * @example ENUM_SWITCH(Example, value, { ENUM_CASE_PAIR(keyA, valueA); ENUM_CASE_PAIR(keyB, valueB); }, defaultValue)
 */
#define ENUM_SWITCH(name, value, cases, defaultValue)       \
    using ENUM_TYPE = name;                                 \
    switch (value) {                                        \
        cases                                               \
        default:                                            \
            return defaultValue;                            \
    }
