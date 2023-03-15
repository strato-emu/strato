// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <algorithm>
#include <random>
#include <span>
#include <frozen/unordered_map.h>
#include <frozen/string.h>
#include <sys/system_properties.h>
#include <type_traits>
#include <xxhash.h>
#include "base.h"
#include "exception.h"

namespace skyline::util {
    /**
     * @brief Concept for any trivial non-container type
     */
    template<typename T>
    concept TrivialObject = std::is_trivially_copyable_v<T> && !requires(T v) { v.data(); };

    namespace detail {
        /**
         * @brief Retrieves the system counter clock frequency
         * @note Some devices report an incorrect value so they need special handling
         */
        inline u64 InitFrequency() {
            char buffer[PROP_VALUE_MAX];
            int len{__system_property_get("ro.product.board", buffer)};
            std::string_view board{buffer, static_cast<size_t>(len)};

            u64 frequency;
            if (board == "s5e9925")             // Exynos 2200
                frequency = 25600000;
            else if (board == "exynos2100")     // Exynos 2100
                frequency = 26000000;
            else if (board == "exynos9810")     // Exynos 9810
                frequency = 26000000;
            else if (board == "s5e8825")        // Exynos 1280
                frequency = 26000000;
            else
                asm volatile("MRS %0, CNTFRQ_EL0" : "=r"(frequency));

            return frequency;
        }
    }

    inline const u64 ClockFrequency{detail::InitFrequency()}; //!< The system counter clock frequency in Hz

    template<i64 TargetFrequency>
    inline i64 GetTimeScaled() {
        u64 frequency{ClockFrequency};
        u64 ticks;
        asm volatile("MRS %0, CNTVCT_EL0" : "=r"(ticks));
        return static_cast<i64>(((ticks / frequency) * TargetFrequency) + (((ticks % frequency) * TargetFrequency + (frequency / 2)) / frequency));
    }
    /**
     * @brief Returns the current time in nanoseconds
     * @return The current time in nanoseconds
     */
    inline i64 GetTimeNs() {
        return GetTimeScaled<constant::NsInSecond>();
    }

    /**
     * @return The current time in guest ticks
     */
    inline u64 GetTimeTicks() {
        constexpr i64 TegraX1ClockFrequency{19200000};    // The clock frequency of the Tegra X1 (19.2 MHz)
        return static_cast<u64>(GetTimeScaled<TegraX1ClockFrequency>());
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
            return static_cast<Return>(item);
    }

    template<typename T>
    concept IsPointerOrUnsignedIntegral = (std::is_unsigned_v<T> && std::is_integral_v<T>) || std::is_pointer_v<T>;

    /**
     * @return The value aligned up to the next multiple
     * @note The multiple **must** be a power of 2
     */
    template<typename TypeVal>
    requires IsPointerOrUnsignedIntegral<TypeVal>
    constexpr TypeVal AlignUp(TypeVal value, size_t multiple) {
        multiple--;
        return ValuePointer<TypeVal>((PointerValue(value) + multiple) & ~(multiple));
    }

    /**
     * @return The value aligned up to the next multiple, the multiple is not restricted to being a power of two (NPOT)
     * @note This will round away from zero for negative numbers
     * @note This is costlier to compute than the power of 2 version, it should be preferred over this when possible
     */
    template<typename TypeVal>
    requires std::is_integral_v<TypeVal> || std::is_pointer_v<TypeVal>
    constexpr TypeVal AlignUpNpot(TypeVal value, ssize_t multiple) {
        return ValuePointer<TypeVal>(((PointerValue(value) + multiple - 1) / multiple) * multiple);
    }

    /**
     * @return The value aligned down to the previous multiple
     * @note The multiple **must** be a power of 2
     */
    template<typename TypeVal>
    requires IsPointerOrUnsignedIntegral<TypeVal>
    constexpr TypeVal AlignDown(TypeVal value, size_t multiple) {
        return ValuePointer<TypeVal>(PointerValue(value) & ~(multiple - 1));
    }

    /**
     * @return If the address is aligned with the multiple
     */
    template<typename TypeVal>
    requires IsPointerOrUnsignedIntegral<TypeVal>
    constexpr bool IsAligned(TypeVal value, size_t multiple) {
        if ((multiple & (multiple - 1)) == 0)
            return !(PointerValue(value) & (multiple - 1U));
        else
            return (PointerValue(value) % multiple) == 0;
    }

    template<typename TypeVal>
    requires IsPointerOrUnsignedIntegral<TypeVal>
    constexpr bool IsPageAligned(TypeVal value) {
        return IsAligned(value, constant::PageSize);
    }

    template<typename TypeVal>
    requires IsPointerOrUnsignedIntegral<TypeVal>
    constexpr bool IsWordAligned(TypeVal value) {
        return IsAligned(value, WORD_BIT / 8);
    }

    /**
     * @return The value of division rounded up to the next integral
     */
    template<typename Type>
    requires std::is_integral_v<Type>
    constexpr Type DivideCeil(Type dividend, Type divisor) {
        return (dividend + divisor - 1) / divisor;
    }

    /**
     * @param string The string to create a magic from
     * @return The magic of the supplied string
     */
    template<typename Type>
    requires std::is_integral_v<Type>
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
    constexpr std::array<u8, Size> HexStringToArray(std::string_view string) {
        if (string.size() != Size * 2)
            throw exception("String size: {} (Expected {})", string.size(), Size);
        std::array<u8, Size> result;
        for (size_t i{}; i < Size; i++) {
            size_t index{i * 2};
            result[i] = static_cast<u8>(HexDigitToNibble(string[index]) << 4) | HexDigitToNibble(string[index + 1]);
        }
        return result;
    }

    template<typename Type>
    requires std::is_integral_v<Type>
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
    constexpr std::array<u8, N> SwapEndianness(std::array<u8, N> in) {
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
     * @brief A fast hash for any trivial object that is designed to be utilized with hash-based containers
     */
    template<typename T> requires std::is_trivial_v<T>
    struct ObjectHash {
        size_t operator()(const T &object) const noexcept {
            return XXH64(&object, sizeof(object), 0);
        }
    };

    /**
     * @brief Selects the largest possible integer type for representing an object alongside providing the size of the object in terms of the underlying type
     */
    template<class T>
    struct IntegerFor {
        using Type = std::conditional_t<sizeof(T) % sizeof(u64) == 0, u64,
                                        std::conditional_t<sizeof(T) % sizeof(u32) == 0, u32,
                                                           std::conditional_t<sizeof(T) % sizeof(u16) == 0, u16, u8>
                                        >
        >;

        static constexpr size_t Count{sizeof(T) / sizeof(Type)};
    };

    namespace detail {
        inline thread_local std::mt19937_64 generator{GetTimeTicks()};
    }

    /**
     * @brief Fills an array with random data from a Mersenne Twister pseudo-random generator
     * @note The generator is seeded with the the current time in ticks
     */
    template<typename T>
    requires std::is_integral_v<T>
    void FillRandomBytes(std::span<T> in) {
        std::uniform_int_distribution<u64> dist(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        std::generate(in.begin(), in.end(), [&]() { return dist(detail::generator); });
    }

    template<TrivialObject T>
    void FillRandomBytes(T &object) {
        FillRandomBytes(std::span(reinterpret_cast<typename IntegerFor<T>::Type *>(&object), IntegerFor<T>::Count));
    }

    template<IsPointerOrUnsignedIntegral T>
    T RandomNumber(T min, T max) {
        std::uniform_int_distribution dist(PointerValue(min), PointerValue(max));
        return ValuePointer<T>(dist(detail::generator));
    }

    /**
     * @brief A temporary shim for C++ 20's bit_cast to make transitioning to it easier
     */
    template<typename To, typename From>
    To BitCast(const From &from) {
        return *reinterpret_cast<const To *>(&from);
    }

    /**
     * @brief A utility type for placing elements by offset in unions rather than relative position in structs
     * @tparam PadType The type of a unit of padding, total size of padding is `sizeof(PadType) * Offset`
     */
    template<size_t Offset, typename ValueType, typename PadType = u8>
    struct OffsetMember {
      private:
        PadType _pad_[Offset];
        ValueType value;

      public:
        OffsetMember &operator=(const ValueType &pValue) {
            value = pValue;
            return *this;
        }

        const auto &operator[](std::size_t index) const {
            return value[index];
        }

        const ValueType &operator*() const {
            return value;
        }

        ValueType &operator*() {
            return value;
        }

        ValueType *operator->() {
            return &value;
        }
    };

    template<typename T, typename... TArgs, size_t... Is>
    std::array<T, sizeof...(Is)> MakeFilledArray(std::index_sequence<Is...>, TArgs &&... args) {
        return {(void(Is), T(args...))...};
    }

    template<typename T, size_t Size, typename... TArgs>
    std::array<T, Size> MakeFilledArray(TArgs &&... args) {
        return MakeFilledArray<T>(std::make_index_sequence<Size>(), std::forward<TArgs>(args)...);
    }

    template<typename T>
    struct IncrementingT {
        using Type = T;
    };

    template<typename T>
    struct IsIncrementingT : std::false_type {};

    template<typename T>
    struct IsIncrementingT<IncrementingT<T>> : std::true_type {};

    template<typename T, size_t Index, typename... TSrcs>
    T MakeMergeElem(TSrcs &&... srcs) {
        auto readElem{[index = Index](auto &&src) -> decltype(auto) {
            using SrcType = std::decay_t<decltype(src)>;
            if constexpr (requires { src[Index]; })
                return src[Index];
            else if constexpr (IsIncrementingT<SrcType>{})
                return static_cast<typename SrcType::Type>(index);
            else
                return src;
        }};
        return T{readElem(std::forward<TSrcs>(srcs))...};
    }

    template<typename T, size_t... Is, typename... TSrcs>
    std::array<T, sizeof...(Is)> MergeInto(std::index_sequence<Is...> seq, TSrcs &&... srcs) {
        return {MakeMergeElem<T, Is>(std::forward<TSrcs>(srcs)...)...};
    }

    /**
     * @brief Constructs {{scalar0, array0[0], array1[0], ... arrayN[0], scalar1}, {scalar0, array0[1], array1[1]. ... arrayN[1], scalar0}, ...}
     */
    template<typename T, size_t Size, typename... TSrcs>
    std::array<T, Size> MergeInto(TSrcs &&... srcs) {
        return MergeInto<T>(std::make_index_sequence<Size>(), std::forward<TSrcs>(srcs)...);
    }

    inline std::string HexDump(std::span<u8> data) {
        return std::accumulate(data.begin(), data.end(), std::string{}, [](std::string str, u8 el) { return std::move(str) + fmt::format("{:02X}", el); });
    }
}
