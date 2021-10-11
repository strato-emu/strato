// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <map>
#include <unordered_map>
#include <span>
#include <list>
#include <vector>
#include <span>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <thread>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <sstream>
#include <memory>
#include <compare>
#include <variant>
#include <random>
#include <sys/mman.h>
#include <fmt/format.h>
#include <frozen/unordered_map.h>
#include <frozen/string.h>
#include <boost/container/small_vector.hpp>
#include <jni.h>

#define FORCE_INLINE __attribute__((always_inline)) // NOLINT(cppcoreguidelines-macro-usage)

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
}

namespace skyline {
    using u128 = __uint128_t; //!< Unsigned 128-bit integer
    using u64 = __uint64_t; //!< Unsigned 64-bit integer
    using u32 = __uint32_t; //!< Unsigned 32-bit integer
    using u16 = __uint16_t; //!< Unsigned 16-bit integer
    using u8 = __uint8_t; //!< Unsigned 8-bit integer
    using i128 = __int128_t; //!< Signed 128-bit integer
    using i64 = __int64_t; //!< Signed 64-bit integer
    using i32 = __int32_t; //!< Signed 32-bit integer
    using i16 = __int16_t; //!< Signed 16-bit integer
    using i8 = __int8_t; //!< Signed 8-bit integer

    using KHandle = u32; //!< The type of a kernel handle
    namespace frz = frozen;

    /**
     * @brief The result of an operation in HOS
     * @url https://switchbrew.org/wiki/Error_codes
     */
    union Result {
        u32 raw{};
        struct __attribute__((packed)) {
            u16 module : 9;
            u16 id : 12;
        };

        /**
         * @note Success is 0, it's the only result that's not specific to a module
         */
        constexpr Result() = default;

        constexpr explicit Result(u16 module, u16 id) : module(module), id(id) {}

        constexpr operator u32() const {
            return raw;
        }
    };

    /**
     * @brief A wrapper around std::optional that also stores a HOS result code
     */
    template<typename ValueType, typename ResultType = Result>
    class ResultValue {
        static_assert(!std::is_same<ValueType, ResultType>::value);

      private:
        std::optional<ValueType> value;

      public:
        ResultType result{};

        constexpr ResultValue(ValueType value) : value(value) {};

        constexpr ResultValue(ResultType result) : result(result) {};

        template<typename U>
        constexpr ResultValue(ResultValue<U> result) : result(result) {};

        constexpr operator ResultType() const {
            return result;
        }

        explicit constexpr operator bool() const {
            return value.has_value();
        }

        constexpr ValueType& operator*()  {
            return *value;
        }

        constexpr ValueType* operator->()  {
            return &*value;
        }
    };

    namespace constant {
        // Time
        constexpr u64 NsInMicrosecond{1000}; //!< The amount of nanoseconds in a microsecond
        constexpr u64 NsInSecond{1000000000}; //!< The amount of nanoseconds in a second
        constexpr u64 NsInMillisecond{1000000}; //!< The amount of nanoseconds in a millisecond
        constexpr u64 NsInDay{86400000000000UL}; //!< The amount of nanoseconds in a day
    }

    namespace util {
        /**
         * @brief A way to implicitly cast all pointers to uintptr_t, this is used for {fmt} as we use 0x{:X} to print pointers
         * @note There's the exception of signed char pointers as they represent C Strings
         * @note This does not cover std::shared_ptr or std::unique_ptr and those will have to be explicitly casted to uintptr_t or passed through fmt::ptr
         */
        template<typename T>
        constexpr auto FmtCast(T object) {
            if constexpr (std::is_pointer<T>::value)
                if constexpr (std::is_same<char, typename std::remove_cv<typename std::remove_pointer<T>::type>::type>::value)
                    return reinterpret_cast<typename std::common_type<char *, T>::type>(object);
                else
                    return reinterpret_cast<const uintptr_t>(object);
            else
                return object;
        }

        /**
         * @brief {fmt}::format but with FmtCast built into it
         */
        template<typename S, typename... Args>
        auto Format(S formatString, Args &&... args) {
            return fmt::format(formatString, FmtCast(args)...);
        }
    }

    /**
     * @brief A wrapper over std::runtime_error with {fmt} formatting
     */
    class exception : public std::runtime_error {
      public:
        template<typename S, typename... Args>
        exception(const S &formatStr, Args &&... args) : runtime_error(fmt::format(formatStr, util::FmtCast(args)...)) {}
    };

    namespace util {
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
        constexpr std::array<u8, Size> HexStringToArray(std::string_view string) {
            if (string.size() != Size * 2)
                throw exception("String size: {} (Expected {})", string.size(), Size);
            std::array<u8, Size> result;
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
            return frz::elsa<frz::string>{}(frz::string(view.data(), view.size()), 0);
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

    /**
     * @brief A custom wrapper over span that adds several useful methods to it
     * @note This class is completely transparent, it implicitly converts from and to span
     */
    template<typename T, size_t Extent = std::dynamic_extent>
    class span : public std::span<T, Extent> {
      public:
        using std::span<T, Extent>::span;
        using std::span<T, Extent>::operator=;

        typedef typename std::span<T, Extent>::element_type element_type;
        typedef typename std::span<T, Extent>::size_type size_type;

        constexpr span(const std::span<T, Extent> &spn) : std::span<T, Extent>(spn) {}

        /**
         * @brief A single-element constructor for a span
         */
        constexpr span(T &spn) : std::span<T, Extent>(&spn, 1) {}

        /**
         * @brief We want to support implicitly casting from std::string_view -> span as it's just a specialization of a data view which span is a generic form of, the opposite doesn't hold true as not all data held by a span is string data therefore the conversion isn't implicit there
         */
        template<typename Traits>
        constexpr span(const std::basic_string_view<T, Traits> &string) : std::span<T, Extent>(const_cast<T *>(string.data()), string.size()) {}

        template<typename Out>
        constexpr Out &as() {
            if constexpr (Extent != std::dynamic_extent && sizeof(T) * Extent >= sizeof(Out))
                return *reinterpret_cast<Out *>(span::data());

            if (span::size_bytes() >= sizeof(Out))
                return *reinterpret_cast<Out *>(span::data());
            throw exception("Span size is less than Out type size (0x{:X}/0x{:X})", span::size_bytes(), sizeof(Out));
        }

        /**
         * @param nullTerminated If true and the string is null-terminated, a view of it will be returned (not including the null terminator itself), otherwise the entire span will be returned as a string view
         */
        constexpr std::string_view as_string(bool nullTerminated = false) {
            return std::string_view(reinterpret_cast<const char *>(span::data()), nullTerminated ? (std::find(span::begin(), span::end(), 0) - span::begin()) : span::size_bytes());
        }

        template<typename Out, size_t OutExtent = std::dynamic_extent, bool SkipAlignmentCheck = false>
        constexpr span<Out> cast() {
            if (SkipAlignmentCheck || util::IsAligned(span::size_bytes(), sizeof(Out)))
                return span<Out, OutExtent>(reinterpret_cast<Out *>(span::data()), span::size_bytes() / sizeof(Out));
            throw exception("Span size not aligned with Out type size (0x{:X}/0x{:X})", span::size_bytes(), sizeof(Out));
        }

        /**
         * @brief Copies data from the supplied span into this one
         * @param amount The amount of elements that need to be copied (in terms of the supplied span), 0 will try to copy the entirety of the other span
         */
        template<typename In, size_t InExtent>
        constexpr void copy_from(const span<In, InExtent> spn, size_type amount = 0) {
            auto size{amount ? amount * sizeof(In) : spn.size_bytes()};
            if (span::size_bytes() < size)
                throw exception("Data being copied is larger than this span");
            std::memmove(span::data(), spn.data(), size);
        }

        /**
         * @brief Implicit type conversion for copy_from, this allows passing in std::vector/std::array in directly is automatically passed by reference which is important for any containers
         */
        template<typename In>
        constexpr void copy_from(const In &in, size_type amount = 0) {
            copy_from(span<typename std::add_const<typename In::value_type>::type>(in), amount);
        }

        /**
         * @return If a supplied span is located entirely inside this span and is effectively a subspan
         */
        constexpr bool contains(const span<T, Extent>& other) const {
            return this->begin() <= other.begin() && this->end() >= other.end();
        }

        /** Comparision operators for equality and binary searches **/

        constexpr bool operator==(const span<T, Extent>& other) const {
            return this->data() == other.data() && this->size() == other.size();
        }

        constexpr bool operator<(const span<T, Extent> &other) const {
            return this->data() < other.data();
        }

        constexpr bool operator<(T* pointer) const {
            return this->data() < pointer;
        }

        constexpr bool operator<(typename std::span<T, Extent>::const_iterator it) const {
            return this->begin() < it;
        }

        /** Base Class Functions that return an instance of it, we upcast them **/
        template<size_t Count>
        constexpr span<T, Count> first() const noexcept {
            return std::span<T, Extent>::template first<Count>();
        }

        template<size_t Count>
        constexpr span<T, Count> last() const noexcept {
            return std::span<T, Extent>::template last<Count>();
        }

        constexpr span<element_type, std::dynamic_extent> first(size_type count) const noexcept {
            return std::span<T, Extent>::first(count);
        }

        constexpr span<element_type, std::dynamic_extent> last(size_type count) const noexcept {
            return std::span<T, Extent>::last(count);
        }

        template<size_t Offset, size_t Count = std::dynamic_extent>
        constexpr auto subspan() const noexcept -> span<T, Count != std::dynamic_extent ? Count : Extent - Offset> {
            return std::span<T, Extent>::template subspan<Offset, Count>();
        }

        constexpr span<T, std::dynamic_extent> subspan(size_type offset, size_type count = std::dynamic_extent) const noexcept {
            return std::span<T, Extent>::subspan(offset, count);
        }
    };

    /**
     * @brief Deduction guides required for arguments to span, CTAD will fail for iterators, arrays and containers without this
     */
    template<typename It, typename End, size_t Extent = std::dynamic_extent>
    span(It, End) -> span<typename std::iterator_traits<It>::value_type, Extent>;
    template<typename T, size_t Size>
    span(T (&)[Size]) -> span<T, Size>;
    template<typename T, size_t Size>
    span(std::array<T, Size> &) -> span<T, Size>;
    template<typename T, size_t Size>
    span(const std::array<T, Size> &) -> span<const T, Size>;
    template<typename Container>
    span(Container &) -> span<typename Container::value_type>;
    template<typename Container>
    span(const Container &) -> span<const typename Container::value_type>;

    /**
     * @brief A deduction guide for overloads required for std::visit with std::variant
     */
    template<class... Ts>
    struct VariantVisitor : Ts ... { using Ts::operator()...; };
    template<class... Ts> VariantVisitor(Ts...) -> VariantVisitor<Ts...>;

    /**
     * @brief A wrapper around writing logs into a log file and logcat using Android Log APIs
     */
    class Logger {
      private:
        std::mutex mutex; //!< Synchronizes all output I/O to ensure there are no races
        std::ofstream logFile; //!< An output stream to the log file
        u64 start; //!< A timestamp in milliseconds for when the logger was started, this is used as the base for all log timestamps

      public:
        enum class LogLevel {
            Error,
            Warn,
            Info,
            Debug,
            Verbose,
        };

        LogLevel configLevel; //!< The minimum level of logs to write

        /**
         * @param path The path of the log file
         * @param configLevel The minimum level of logs to write
         */
        Logger(const std::string &path, LogLevel configLevel);

        /**
         * @brief Writes the termination message to the log file
         */
        ~Logger();

        /**
         * @brief Update the tag in log messages with a new thread name
         */
        static void UpdateTag();

        void Write(LogLevel level, const std::string &str);

        /**
         * @brief A wrapper around a string which captures the calling function using Clang source location builtins
         * @note A function needs to be declared for every argument template specialization as CTAD cannot work with implicit casting
         * @url https://clang.llvm.org/docs/LanguageExtensions.html#source-location-builtins
         */
        template<typename S>
        struct FunctionString {
            S string;
            const char *function;

            FunctionString(S string, const char *function = __builtin_FUNCTION()) : string(std::move(string)), function(function) {}

            std::string operator*() {
                return std::string(function) + ": " + std::string(string);
            }
        };

        template<typename... Args>
        void Error(FunctionString<const char*> formatString, Args &&... args) {
            if (LogLevel::Error <= configLevel)
                Write(LogLevel::Error, fmt::format(*formatString, util::FmtCast(args)...));
        }

        template<typename... Args>
        void Error(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Error <= configLevel)
                Write(LogLevel::Error, fmt::format(*formatString, util::FmtCast(args)...));
        }

        template<typename S, typename... Args>
        void ErrorNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Error <= configLevel)
                Write(LogLevel::Error, fmt::format(formatString, util::FmtCast(args)...));
        }

        template<typename... Args>
        void Warn(FunctionString<const char*> formatString, Args &&... args) {
            if (LogLevel::Warn <= configLevel)
                Write(LogLevel::Warn, fmt::format(*formatString, util::FmtCast(args)...));
        }

        template<typename... Args>
        void Warn(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Warn <= configLevel)
                Write(LogLevel::Warn, fmt::format(*formatString, util::FmtCast(args)...));
        }

        template<typename S, typename... Args>
        void WarnNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Warn <= configLevel)
                Write(LogLevel::Warn, fmt::format(formatString, util::FmtCast(args)...));
        }

        template<typename... Args>
        void Info(FunctionString<const char*> formatString, Args &&... args) {
            if (LogLevel::Info <= configLevel)
                Write(LogLevel::Info, fmt::format(*formatString, util::FmtCast(args)...));
        }

        template<typename... Args>
        void Info(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Info <= configLevel)
                Write(LogLevel::Info, fmt::format(*formatString, util::FmtCast(args)...));
        }

        template<typename S, typename... Args>
        void InfoNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Info <= configLevel)
                Write(LogLevel::Info, fmt::format(formatString, util::FmtCast(args)...));
        }

        template<typename... Args>
        void Debug(FunctionString<const char*> formatString, Args &&... args) {
            if (LogLevel::Debug <= configLevel)
                Write(LogLevel::Debug, fmt::format(*formatString, util::FmtCast(args)...));
        }

        template<typename... Args>
        void Debug(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Debug <= configLevel)
                Write(LogLevel::Debug, fmt::format(*formatString, util::FmtCast(args)...));
        }

        template<typename S, typename... Args>
        void DebugNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Debug <= configLevel)
                Write(LogLevel::Debug, fmt::format(formatString, util::FmtCast(args)...));
        }

        template<typename... Args>
        void Verbose(FunctionString<const char*> formatString, Args &&... args) {
            if (LogLevel::Verbose <= configLevel)
                Write(LogLevel::Verbose, fmt::format(*formatString, util::FmtCast(args)...));
        }

        template<typename... Args>
        void Verbose(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Verbose <= configLevel)
                Write(LogLevel::Verbose, fmt::format(*formatString, util::FmtCast(args)...));
        }

        template<typename S, typename... Args>
        void VerboseNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Verbose <= configLevel)
                Write(LogLevel::Verbose, fmt::format(formatString, util::FmtCast(args)...));
        }
    };

    class Settings;
    namespace nce {
        class NCE;
        struct ThreadContext;
    }
    class JvmManager;
    namespace gpu {
        class GPU;
    }
    namespace soc {
        class SOC;
    }
    namespace kernel {
        namespace type {
            class KProcess;
            class KThread;
        }
        class Scheduler;
        class OS;
    }
    namespace audio {
        class Audio;
    }
    namespace input {
        class Input;
    }
    namespace loader {
        class Loader;
    }

    /**
     * @brief The state of the entire emulator is contained within this class, all objects related to emulation are tied into it
     */
    struct DeviceState {
        DeviceState(kernel::OS *os, std::shared_ptr<JvmManager> jvmManager, std::shared_ptr<Settings> settings, std::shared_ptr<Logger> logger);

        ~DeviceState();

        kernel::OS *os;
        std::shared_ptr<JvmManager> jvm;
        std::shared_ptr<Settings> settings;
        std::shared_ptr<Logger> logger;
        std::shared_ptr<loader::Loader> loader;
        std::shared_ptr<kernel::type::KProcess> process{};
        static thread_local inline std::shared_ptr<kernel::type::KThread> thread{}; //!< The KThread of the thread which accesses this object
        static thread_local inline nce::ThreadContext *ctx{}; //!< The context of the guest thread for the corresponding host thread
        std::shared_ptr<gpu::GPU> gpu;
        std::shared_ptr<soc::SOC> soc;
        std::shared_ptr<audio::Audio> audio;
        std::shared_ptr<nce::NCE> nce;
        std::shared_ptr<kernel::Scheduler> scheduler;
        std::shared_ptr<input::Input> input;
    };
}
