// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <concepts>
#include <common.h>
#include <services/nvdrv/types.h>

namespace skyline::service::nvdrv::deserialisation {
    // IOCTL parameters can't be pointers and must be trivially copyable
    template<typename T>
    concept BufferData = std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>;

    // Input IOCTL template types
    template<typename T> requires BufferData<T>
    using In = const T;

    template<typename>
    struct IsIn : std::false_type {};

    template<typename T>
    struct IsIn<In<T>> : std::true_type {};


    // Input/Output IOCTL template types
    template<typename T> requires BufferData<T>
    using InOut = T &;

    template<typename>
    struct IsInOut : std::false_type {};

    template<typename T>
    struct IsInOut<InOut<T>> : std::true_type {};

    template<typename T> requires IsInOut<T>::value
    using RemoveInOut = typename std::remove_reference<T>::type;


    // Output IOCTL template types
    template<typename T> requires BufferData<T>
    class Out {
      private:
        T &ref;

      public:
        Out(T &ref) : ref(ref) {}

        using ValueType = T;

        Out& operator=(T val) {
            ref = std::move(val);
            return *this;
        }
    };

    template<typename>
    struct IsOut : std::false_type {};

    template<typename T>
    struct IsOut<Out<T>> : std::true_type {};

    template<typename T> requires IsOut<T>::value
    using RemoveOut = typename T::ValueType;

    // Padding template types
    template<typename T, size_t Count = 1> requires BufferData<T>
    struct Pad {
        static constexpr auto Bytes{Count * sizeof(T)};
    };

    template<typename>
    struct IsPad : std::false_type {};

    template<typename T, size_t TCount>
    struct IsPad<Pad<T, TCount>> : std::true_type {};

    // Save to slot template types
    constexpr size_t NumSaveSlots{10}; //!< Number of slots to save temporary values in during argument decoding

    template<typename T, size_t TSaveSlot> requires (std::is_integral_v<T> && TSaveSlot < NumSaveSlots)
    struct Save {
        using SaveType = T;

        static constexpr auto SaveSlot{TSaveSlot};
    };

    template<typename>
    struct IsSave : std::false_type {};

    template<typename T, size_t SaveSlot>
    struct IsSave<Save<T, SaveSlot>> : std::true_type {};

    template<typename T> requires IsSave<T>::value
    using RemoveSave = typename T::SaveType;

    // Span template types
    template<typename T> requires BufferData<T>
    using AutoSizeSpan = span<T>;

    template<typename>
    struct IsAutoSizeSpan : std::false_type {};

    template<typename T>
    struct IsAutoSizeSpan<AutoSizeSpan<T>> : std::true_type {};

    template<typename T> requires IsAutoSizeSpan<T>::value
    using RemoveAutoSizeSpan = typename T::element_type;

    template<typename T, size_t TSaveSlot> requires (BufferData<T> && TSaveSlot < NumSaveSlots)
    struct SlotSizeSpan {
        static constexpr auto SaveSlot{TSaveSlot};

        using ValueType = T;
    };

    template<typename>
    struct IsSlotSizeSpan : std::false_type {};

    template<typename T, size_t SaveSlot>
    struct IsSlotSizeSpan<SlotSizeSpan<T, SaveSlot>> : std::true_type {};

    template<typename T> requires IsSlotSizeSpan<T>::value
    using RemoveSlotSizeSpan = typename T::ValueType;

    /**
     * @brief Describes an IOCTL as a type for use in deserialisation
     */
    template <bool TOut, bool TIn, u16 TSize, i8 TMagic, u8 TFunction>
    struct MetaIoctlDescriptor {
        static constexpr auto Out{TOut};
        static constexpr auto In{TIn};
        static constexpr auto Size{TSize};
        static constexpr auto Magic{TMagic};
        static constexpr auto Function{TFunction};

        constexpr operator IoctlDescriptor() const {
            return {Out, In, Size, Magic, Function};
        }

        constexpr static u32 Raw() {
            u32 raw{Function};

            int offset{8};
            raw |= static_cast<u32>(Magic << offset); offset += 8;
            raw |= static_cast<u32>(Size << offset); offset += 14;
            raw |= (In ? 1U : 0U) << offset; offset++;
            raw |= (Out ? 1U : 0U) << offset; offset++;
            return raw;
        }
    };

    /**
     * @brief Describes a variable length IOCTL as a type for use in deserialisation
     */
    template <bool TOut, bool TIn, i8 TMagic, u8 TFunction>
    struct MetaVariableIoctlDescriptor {
        static constexpr auto Out{TOut};
        static constexpr auto In{TIn};
        static constexpr auto Size{std::dynamic_extent};
        static constexpr auto Magic{TMagic};
        static constexpr auto Function{TFunction};

        constexpr static u32 Raw() {
            u32 raw{Function};

            i8 offset{8};
            raw |= static_cast<u32>(Magic << offset); offset += 8;
            offset += 14; // Use a 0 size for raw
            raw |= (In ? 1U : 0U) << offset; offset++;
            raw |= (Out ? 1U : 0U) << offset; offset++;
            return raw;
        }
    };
}
