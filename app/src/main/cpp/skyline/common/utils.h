// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <random>
#include <frozen/unordered_map.h>
#include <frozen/string.h>
#include "base.h"

namespace skyline::util {
    /**
     * @brief Returns the current time in nanoseconds
     * @return The current time in nanoseconds
     */
    inline u64 GetTimeNs() {
        u64 frequency;
        asm("MRS %0, CNTFRQ_EL0" : "=r"(frequency));
        u64 ticks;
        asm("MRS %0, CNTVCT_EL0" : "=r"(ticks));
        return ((ticks / frequency) * constant::NsInSecond) + (((ticks % frequency) * constant::NsInSecond + (frequency / 2)) / frequency);
    }

    /**
     * @brief Returns the current time in arbitrary ticks
     * @return The current time in ticks
     */
    inline u64 GetTimeTicks() {
        u64 ticks;
        asm("MRS %0, CNTVCT_EL0" : "=r"(ticks));
        return ticks;
    }

    /**
     * @brief A way to implicitly convert a pointer to uintptr_t and leave it unaffected if it isn't a pointer
     */
    template<typename T>
    constexpr T PointerValue(T item) {
        return item;
    }

    template<typename T>
    uintptr_t PointerValue(T *item) {
        return reinterpret_cast<uintptr_t>(item);
    }

    /**
     * @brief A way to implicitly convert an integral to a pointer, if the return type is a pointer
     */
    template<typename Return, typename T>
    constexpr Return ValuePointer(T item) {
        if constexpr (std::is_pointer<Return>::value)
            return reinterpret_cast<Return>(item);
        else
            return item;
    }

    /**
     * @return The value aligned up to the next multiple
     * @note The multiple needs to be a power of 2
     */
    template<typename TypeVal, typename TypeMul>
    constexpr TypeVal AlignUp(TypeVal value, TypeMul multiple) {
        multiple--;
        return ValuePointer<TypeVal>((PointerValue(value) + multiple) & ~(multiple));
    }

    /**
     * @return The value aligned down to the previous multiple
     * @note The multiple needs to be a power of 2
     */
    template<typename TypeVal, typename TypeMul>
    constexpr TypeVal AlignDown(TypeVal value, TypeMul multiple) {
        return ValuePointer<TypeVal>(PointerValue(value) & ~(multiple - 1));
    }

    /**
     * @return If the address is aligned with the multiple
     */
    template<typename TypeVal, typename TypeMul>
    constexpr bool IsAligned(TypeVal value, TypeMul multiple) {
        if ((multiple & (multiple - 1)) == 0)
            return !(PointerValue(value) & (multiple - 1U));
        else
            return (PointerValue(value) % multiple) == 0;
    }

    /**
     * @return If the value is page aligned
     */
    template<typename TypeVal>
    constexpr bool PageAligned(TypeVal value) {
        return IsAligned(value, PAGE_SIZE);
    }

    /**
     * @return If the value is word aligned
     */
    template<typename TypeVal>
    constexpr bool WordAligned(TypeVal value) {
        return IsAligned(value, WORD_BIT / 8);
    }

    /**
     * @param string The string to create a magic from
     * @return The magic of the supplied string
     */
    template<typename Type>
    constexpr Type MakeMagic(std::string_view string) {
        Type object{};
        size_t offset{};

        for (auto &character : string) {
            object |= static_cast<Type>(character) << offset;
            offset += sizeof(character) * 8;
        }

        return object;
    }

    constexpr u8 HexDigitToNibble(char digit) {
        if (digit >= '0' && digit <= '9')
            return digit - '0';
        else if (digit >= 'a' && digit <= 'f')
            return digit - 'a' + 10;
        else if (digit >= 'A' && digit <= 'F')
            return digit - 'A' + 10;
        throw exception("Invalid hex character: '{}'", digit);
    }

    template<size_t Size>
    constexpr std::array <u8, Size> HexStringToArray(std::string_view string) {
        if (string.size() != Size * 2)
            throw exception("String size: {} (Expected {})", string.size(), Size);
        std::array <u8, Size> result;
        for (size_t i{}; i < Size; i++) {
            size_t index{i * 2};
            result[i] = (HexDigitToNibble(string[index]) << 4) | HexDigitToNibble(string[index + 1]);
        }
        return result;
    }

    template<typename Type>
    constexpr Type HexStringToInt(std::string_view string) {
        if (string.size() > sizeof(Type) * 2)
            throw exception("String size larger than type: {} (sizeof(Type): {})", string.size(), sizeof(Type));
        Type result{};
        size_t offset{(sizeof(Type) * 8) - 4};
        for (size_t index{}; index < string.size(); index++, offset -= 4) {
            char digit{string[index]};
            if (digit >= '0' && digit <= '9')
                result |= static_cast<Type>(digit - '0') << offset;
            else if (digit >= 'a' && digit <= 'f')
                result |= static_cast<Type>(digit - 'a' + 10) << offset;
            else if (digit >= 'A' && digit <= 'F')
                result |= static_cast<Type>(digit - 'A' + 10) << offset;
            else
                break;
        }
        return result >> (offset + 4);
    }

    template<size_t N>
    constexpr std::array <u8, N> SwapEndianness(std::array <u8, N> in) {
        std::reverse(in.begin(), in.end());
        return in;
    }

    constexpr u64 SwapEndianness(u64 in) {
        return __builtin_bswap64(in);
    }

    constexpr u32 SwapEndianness(u32 in) {
        return __builtin_bswap32(in);
    }

    constexpr u16 SwapEndianness(u16 in) {
        return __builtin_bswap16(in);
    }

    /**
     * @brief A compile-time hash function as std::hash isn't constexpr
     */
    constexpr std::size_t Hash(std::string_view view) {
        return frozen::elsa<frozen::string>{}(frozen::string(view.data(), view.size()), 0);
    }

    /**
     * @brief Selects the largest possible integer type for representing an object alongside providing the size of the object in terms of the underlying type
     */
    template<class T>
    struct IntegerFor {
        using type = std::conditional_t<sizeof(T) % sizeof(u64) == 0, u64,
            std::conditional_t<sizeof(T) % sizeof(u32) == 0, u32,
                               std::conditional_t<sizeof(T) % sizeof(u16) == 0, u16, u8>
            >
        >;

        static constexpr size_t count{sizeof(T) / sizeof(type)};
    };

    namespace detail {
        static thread_local std::mt19937_64 generator{GetTimeTicks()};
    }

    /**
     * @brief Fills an array with random data from a Mersenne Twister pseudo-random generator
     * @note The generator is seeded with the the current time in ticks
     */
    template<typename T> requires (std::is_integral_v<T>)
    void FillRandomBytes(std::span<T> in) {
        std::independent_bits_engine<std::mt19937_64, std::numeric_limits<T>::digits, T> gen(detail::generator);
        std::generate(in.begin(), in.end(), gen);
    }

    template<class T> requires (!std::is_integral_v<T> && std::is_trivially_copyable_v<T>)
    void FillRandomBytes(T &object) {
        FillRandomBytes(std::span(reinterpret_cast<typename IntegerFor<T>::type *>(&object), IntegerFor<T>::count));
    }

    /**
     * @brief A temporary shim for C++ 20's bit_cast to make transitioning to it easier
     */
    template<typename To, typename From>
    To BitCast(const From& from) {
        return *reinterpret_cast<const To*>(&from);
    }
}
