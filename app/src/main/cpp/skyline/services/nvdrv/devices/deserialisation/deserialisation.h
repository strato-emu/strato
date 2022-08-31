// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <type_traits>
#include <common.h>
#include "types.h"

namespace skyline::service::nvdrv::deserialisation {
    template<typename Desc, typename ArgType> requires (Desc::In && IsIn<ArgType>::value)
    constexpr ArgType DecodeArgument(span<u8, Desc::Size> buffer, size_t &offset, std::array<size_t, NumSaveSlots> &saveSlots) {
        auto out{buffer.subspan(offset).template as<ArgType, true>()};
        offset += sizeof(ArgType);
        return out;
    }

    template<typename Desc, typename ArgType> requires (Desc::Out && Desc::In && IsInOut<ArgType>::value)
    constexpr ArgType DecodeArgument(span<u8, Desc::Size> buffer, size_t &offset, std::array<size_t, NumSaveSlots> &saveSlots) {
        auto &out{buffer.subspan(offset).template as<RemoveInOut<ArgType>, true>()};
        offset += sizeof(RemoveInOut<ArgType>);
        return out;
    }

    template<typename Desc, typename ArgType> requires (Desc::Out && IsOut<ArgType>::value)
    constexpr ArgType DecodeArgument(span<u8, Desc::Size> buffer, size_t &offset, std::array<size_t, NumSaveSlots> &saveSlots) {
        auto out{Out(buffer.subspan(offset).template as<RemoveOut<ArgType>, true>())};
        offset += sizeof(RemoveOut<ArgType>);
        return out;
    }

    template<typename Desc, typename ArgType> requires (IsSlotSizeSpan<ArgType>::value)
    constexpr auto DecodeArgument(span<u8, Desc::Size> buffer, size_t &offset, std::array<size_t, NumSaveSlots> &saveSlots) {
        size_t bytes{saveSlots[ArgType::SaveSlot] * sizeof(RemoveSlotSizeSpan<ArgType>)};
        auto out{buffer.subspan(offset, bytes).template cast<RemoveSlotSizeSpan<ArgType>, std::dynamic_extent, true>()};
        offset += bytes;

        // Return a simple `span` as that will be the function argument type as opposed to `SlotSizeSpan`
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
    constexpr auto DecodeArgumentsImpl(span<u8, Desc::Size> buffer, size_t &offset, std::array<size_t, NumSaveSlots> &saveSlots) {
        if constexpr (IsAutoSizeSpan<ArgType>::value) {
            // AutoSizeSpan needs to be the last argument
            static_assert(sizeof...(ArgTypes) == 0);
            return make_ref_tuple(buffer.subspan(offset).template cast<RemoveAutoSizeSpan<ArgType>, std::dynamic_extent, true>());
        } else if constexpr (IsPad<ArgType>::value) {
            offset += ArgType::Bytes;
            if constexpr(sizeof...(ArgTypes) == 0) {
                return std::tuple{};
            } else {
                return DecodeArgumentsImpl<Desc, ArgTypes...>(buffer, offset, saveSlots);
            }
        } else if constexpr (IsSave<ArgType>::value) {
            saveSlots[ArgType::SaveSlot] = buffer.subspan(offset).template as<RemoveSave<ArgType>, true>();
            offset += sizeof(RemoveSave<ArgType>);
            return DecodeArgumentsImpl<Desc, ArgTypes...>(buffer, offset, saveSlots);
        } else {
            if constexpr(sizeof...(ArgTypes) == 0) {
                return make_ref_tuple(DecodeArgument<Desc, ArgType>(buffer, offset, saveSlots));
            } else {
                return std::tuple_cat(make_ref_tuple(DecodeArgument<Desc, ArgType>(buffer, offset, saveSlots)),
                                                     DecodeArgumentsImpl<Desc, ArgTypes...>(buffer, offset, saveSlots));
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
        std::array<size_t, NumSaveSlots> saveSlots{}; // No need to zero init as used slots will always be loaded first
        return DecodeArgumentsImpl<Desc, ArgTypes...>(buffer, offset, saveSlots);
    }
}