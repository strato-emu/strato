// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <type_traits>
#include <common.h>
#include "types.h"

namespace skyline::service::nvdrv::deserialisation {
    template<typename Desc, typename ArgType> requires (Desc::In && IsIn<ArgType>::value)
    constexpr ArgType DecodeArgument(span<u8, Desc::Size> buffer, size_t &offset) {
        auto out{buffer.subspan(offset).template as<ArgType>()};
        offset += sizeof(ArgType);
        return out;
    }

    template<typename Desc, typename ArgType> requires (Desc::Out && Desc::In && IsInOut<ArgType>::value)
    constexpr ArgType DecodeArgument(span<u8, Desc::Size> buffer, size_t &offset) {
        auto &out{buffer.subspan(offset).template as<RemoveInOut<ArgType>>()};
        offset += sizeof(RemoveInOut<ArgType>);
        return out;
    }

    template<typename Desc, typename ArgType> requires (Desc::Out && IsOut<ArgType>::value)
    constexpr ArgType DecodeArgument(span<u8, Desc::Size> buffer, size_t &offset) {
        auto out{Out(buffer.subspan(offset).template as<RemoveOut<ArgType>>())};
        offset += sizeof(RemoveOut<ArgType>);
        return out;
    }

    /**
     * @brief Creates a reference preserving tuple of the given types
     */
    template <typename...Ts>
    constexpr auto make_ref_tuple(Ts&&...ts) {
        return std::tuple<Ts...>{std::forward<Ts>(ts)...};
    }

    template<typename Desc, typename ArgType, typename... ArgTypes>
    constexpr auto DecodeArgumentsImpl(span<u8, Desc::Size> buffer, size_t &offset) {
        if constexpr (IsAutoSizeSpan<ArgType>::value) {
            // AutoSizeSpan needs to be the last argument
            static_assert(sizeof...(ArgTypes) == 0);
            size_t bytes{buffer.size() - offset};
            size_t extent{bytes / sizeof(RemoveAutoSizeSpan<ArgType>)};
            return make_ref_tuple(buffer.subspan(offset, extent * sizeof(RemoveAutoSizeSpan<ArgType>)).template cast<RemoveAutoSizeSpan<ArgType>>());
        } else if constexpr (IsPad<ArgType>::value) {
            offset += ArgType::Bytes;
            return DecodeArgumentsImpl<Desc, ArgTypes...>(buffer, offset);
        } else {
            if constexpr(sizeof...(ArgTypes) == 0) {
                return make_ref_tuple(DecodeArgument<Desc, ArgType>(buffer, offset));
            } else {
                return std::tuple_cat(make_ref_tuple(DecodeArgument<Desc, ArgType>(buffer, offset)),
                                                     DecodeArgumentsImpl<Desc, ArgTypes...>(buffer, offset));
            }
        }
    }

    /**
     * @brief This fancy thing takes a varadic template of argument types and uses it to deserialise the given buffer
     * @tparam Desc A MetaIoctlDescriptor or MetaVariableIoctlDescriptor corresponding to the IOCTL that takes these arguments
     * @return A tuple containing the arguments to be passed to the IOCTL's handler
     */
    template<typename Desc, typename... ArgTypes>
    constexpr auto DecodeArguments(span<u8, Desc::Size> buffer) {
        size_t offset{};
        return DecodeArgumentsImpl<Desc, ArgTypes...>(buffer, offset);
    }
}