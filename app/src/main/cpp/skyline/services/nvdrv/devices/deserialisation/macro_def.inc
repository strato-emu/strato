// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "deserialisation.h"

#define INLINE_IOCTL_HANDLER_FUNC(type, name, cases)                                        \
    PosixResult name::type(IoctlDescriptor cmd, span<u8> buffer, span<u8> inlineBuffer) {   \
        using className = name;                                                             \
        switch (cmd.raw) {                                                                  \
            cases                                                                           \
            default:                                                                        \
                return PosixResult::InappropriateIoctlForDevice;                            \
        }                                                                                   \
    }

#define VARIABLE_IOCTL_HANDLER_FUNC(name, cases, variableCases)          \
    PosixResult name::Ioctl(IoctlDescriptor cmd, span<u8> buffer) {      \
        using className = name;                                          \
        switch (cmd.raw) {                                               \
            cases                                                        \
            default:                                                     \
                cmd.size = 0;                                            \
                switch (cmd.raw) {                                       \
                    variableCases                                        \
                    default:                                             \
                        return PosixResult::InappropriateIoctlForDevice; \
                }                                                        \
        }                                                                \
    }

#define IOCTL_HANDLER_FUNC(name, cases)                 \
    PosixResult name::Ioctl(IoctlDescriptor cmd, span<u8> buffer) { \
        using className = name;                                     \
        switch (cmd.raw) {                                          \
            cases                                                   \
            default:                                                \
                return PosixResult::InappropriateIoctlForDevice;    \
        }                                                           \
    }

#define IOCTL_CASE_ARGS_I(out, in, size, magic, function, name, ...)                      \
    case MetaIoctlDescriptor<out, in, size, magic, function>::Raw(): {                    \
        using IoctlType = MetaIoctlDescriptor< out, in, size, magic, function>;           \
        auto args = DecodeArguments<IoctlType, __VA_ARGS__>(buffer.subspan<0, size>());   \
        return std::apply(&className::name, std::tuple_cat(std::make_tuple(this), args)); \
    }

#define IOCTL_CASE_NOARGS_I(out, in, size, magic, function, name)       \
    case MetaIoctlDescriptor<out, in, size, magic, function>::Raw():    \
        return className::name();

#define IOCTL_CASE_RESULT_I(out, in, size, magic, function, result)     \
    case MetaIoctlDescriptor<out, in, size, magic, function>::Raw():    \
        return result;

#define IOCTL_CASE_ARGS(...) IOCTL_CASE_ARGS_I(__VA_ARGS__)
#define IOCTL_CASE_NOARGS(...) IOCTL_CASE_NOARGS_I(__VA_ARGS__)
#define IOCTL_CASE_RESULT(...) IOCTL_CASE_RESULT_I(__VA_ARGS__)


#define VARIABLE_IOCTL_CASE_ARGS_I(out, in, magic, function, name, ...)                   \
    case MetaVariableIoctlDescriptor<out, in, magic, function>::Raw(): {                  \
        using IoctlType = MetaVariableIoctlDescriptor<out, in, magic, function>;          \
        auto args = DecodeArguments<IoctlType, __VA_ARGS__>(buffer);                      \
        return std::apply(&className::name, std::tuple_cat(std::make_tuple(this), args)); \
    }

#define VARIABLE_IOCTL_CASE_ARGS(...) VARIABLE_IOCTL_CASE_ARGS_I(__VA_ARGS__)


#define INLINE_IOCTL_CASE_ARGS_I(out, in, size, magic, function, name, ...)                             \
    case MetaIoctlDescriptor<out, in, size, magic, function>::Raw(): {                                  \
        using IoctlType = MetaIoctlDescriptor< out, in, size, magic, function>;                         \
        auto args = DecodeArguments<IoctlType, __VA_ARGS__>(buffer.subspan<0, size>());                 \
        return std::apply(&className::name, std::tuple_cat(std::make_tuple(this, inlineBuffer), args)); \
    }

#define INLINE_IOCTL_CASE_ARGS(...) INLINE_IOCTL_CASE_ARGS_I(__VA_ARGS__)

#define IN false, true
#define OUT true, false
#define INOUT true, true
#define NONE false, false
#define SIZE(size) size
#define FUNC(func) func
#define MAGIC(magic) magic
#define ARGS(...) __VA_ARGS__
