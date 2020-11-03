// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <map>
#include <unordered_map>
#include <span>
#include <vector>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <thread>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <sstream>
#include <memory>
#include <sys/mman.h>
#include <fmt/format.h>
#include <frozen/unordered_map.h>
#include <frozen/string.h>
#include <jni.h>

#define FORCE_INLINE __attribute__((always_inline)) inline // NOLINT(cppcoreguidelines-macro-usage)

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
         * @note Success is 0, 0 - it is the only error that's not specific to a module
         */
        Result() = default;

        constexpr Result(u16 module, u16 id) {
            this->module = module;
            this->id = id;
        }

        constexpr operator u32() const {
            return raw;
        }
    };

    namespace constant {
        // Display
        constexpr u16 HandheldResolutionW{1280}; //!< The width component of the handheld resolution
        constexpr u16 HandheldResolutionH{720}; //!< The height component of the handheld resolution
        constexpr u16 DockedResolutionW{1920}; //!< The width component of the docked resolution
        constexpr u16 DockedResolutionH{1080}; //!< The height component of the docked resolution
        // Time
        constexpr u64 NsInSecond{1000000000}; //!< The amount of nanoseconds in a second
    }

    namespace util {
        /**
         * @brief A way to implicitly cast all pointers to u64s, this is used for libfmt as we use 0x{:X} to print pointers
         * @note There's the exception of signed char pointers as they represent C Strings
         * @note This does not cover std::shared_ptr or std::unique_ptr and those will have to be explicitly casted to u64 or passed through fmt::ptr
         */
        template<class T>
        constexpr auto FmtCast(T object) {
            if constexpr (std::is_pointer<T>::value)
                if constexpr (std::is_same<char, typename std::remove_cv<typename std::remove_pointer<T>::type>::type>::value)
                    return reinterpret_cast<typename std::common_type<char *, T>::type>(object);
                else
                    return reinterpret_cast<const u64>(object);
            else
                return object;
        }
    }

    /**
     * @brief A wrapper over std::runtime_error with libfmt formatting
     */
    class exception : public std::runtime_error {
      public:
        /**
         * @param formatStr The exception string to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        inline exception(const S &formatStr, Args &&... args) : runtime_error(fmt::format(formatStr, util::FmtCast(args)...)) {}
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
         * @brief A way to implicitly convert a pointer to size_t and leave it unaffected if it isn't a pointer
         */
        template<class T>
        T PointerValue(T item) {
            return item;
        }

        template<class T>
        size_t PointerValue(T *item) {
            return reinterpret_cast<uintptr_t>(item);
        }

        /**
         * @return The value aligned up to the next multiple
         * @note The multiple needs to be a power of 2
         */
        template<typename TypeVal, typename TypeMul>
        constexpr TypeVal AlignUp(TypeVal value, TypeMul multiple) {
            multiple--;
            return (PointerValue(value) + multiple) & ~(multiple);
        }

        /**
         * @return The value aligned down to the previous multiple
         * @note The multiple needs to be a power of 2
         */
        template<typename TypeVal, typename TypeMul>
        constexpr TypeVal AlignDown(TypeVal value, TypeMul multiple) {
            return PointerValue(value) & ~(multiple - 1);
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
                throw exception("Invalid size");
            std::array<u8, Size> result;
            for (size_t i{}; i < Size; i++) {
                size_t index{i * 2};
                result[i] = (HexDigitToNibble(string[index]) << 4) | HexDigitToNibble(string[index + 1]);
            }
            return result;
        }

        template<class Type>
        constexpr Type HexStringToInt(std::string_view string) {
            Type result{};
            size_t offset{(sizeof(Type) * 8) - 4};
            for (size_t index{}; index < std::min(sizeof(Type) * 2, string.size()); index++, offset -= 4) {
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

        /**
         * @brief A compile-time hash function as std::hash isn't constexpr
         */
        constexpr std::size_t Hash(std::string_view view) {
            return frz::elsa<frz::string>{}(frz::string(view.data(), view.size()), 0);
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

        typedef typename std::span<T, Extent>::element_type elementType;
        typedef typename std::span<T, Extent>::index_type indexType;

        constexpr span(const std::span<T, Extent> &spn) : std::span<T, Extent>(spn) {}

        /**
         * @brief We want to support implicitly casting from std::string_view -> span as it is just a specialization of a data view which span is a generic form of, the opposite doesn't hold true as not all data held by a span is string data therefore the conversion isn't implicit there
         */
        template<class Traits>
        constexpr span(const std::basic_string_view<T, Traits> &string) : std::span<T, Extent>(const_cast<T *>(string.data()), string.size()) {}

        template<typename Out>
        constexpr Out &as() {
            if (span::size_bytes() >= sizeof(Out))
                return *reinterpret_cast<Out *>(span::data());
            throw exception("Span size is less than Out type size (0x{:X}/0x{:X})", span::size_bytes(), sizeof(Out));
        }

        /**
         * @param nullTerminated If true and the string is null-terminated, a view of it will be returned (not including the null terminator itself), otherwise the entire span will be returned as a string view
         */
        constexpr std::string_view as_string(bool nullTerminated = false) {
            return std::string_view(reinterpret_cast<char *>(span::data()), nullTerminated ? (std::find(span::begin(), span::end(), 0) - span::begin()) : span::size_bytes());
        }

        template<typename Out, size_t OutExtent = std::dynamic_extent>
        constexpr span<Out> cast() {
            if (util::IsAligned(span::size_bytes(), sizeof(Out)))
                return span<Out, OutExtent>(reinterpret_cast<Out *>(span::data()), span::size_bytes() / sizeof(Out));
            throw exception("Span size not aligned with Out type size (0x{:X}/0x{:X})", span::size_bytes(), sizeof(Out));
        }

        /**
         * @brief Copies data from the supplied span into this one
         * @param amount The amount of elements that need to be copied (in terms of the supplied span), 0 will try to copy the entirety of the other span
         */
        template<typename In, size_t InExtent>
        constexpr void copy_from(const span<In, InExtent> spn, indexType amount = 0) {
            auto size{amount ? amount * sizeof(In) : spn.size_bytes()};
            if (span::size_bytes() < size)
                throw exception("Data being copied is larger than this span");
            std::memmove(span::data(), spn.data(), size);
        }

        /**
         * @brief Implicit type conversion for copy_from, this allows passing in std::vector/std::array in directly is automatically passed by reference which is important for any containers
         */
        template<typename In>
        constexpr void copy_from(const In &in, indexType amount = 0) {
            copy_from(span<typename std::add_const<typename In::value_type>::type>(in), amount);
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

        constexpr span<elementType, std::dynamic_extent> first(indexType count) const noexcept {
            return std::span<T, Extent>::first(count);
        }

        constexpr span<elementType, std::dynamic_extent> last(indexType count) const noexcept {
            return std::span<T, Extent>::last(count);
        }

        template<size_t Offset, size_t Count = std::dynamic_extent>
        constexpr auto subspan() const noexcept -> span<T, Count != std::dynamic_extent ? Count : Extent - Offset> {
            return std::span<T, Extent>::template subspan<Offset, Count>();
        }

        constexpr span<T, std::dynamic_extent> subspan(indexType offset, indexType count = std::dynamic_extent) const noexcept {
            return std::span<T, Extent>::subspan(offset, count);
        }
    };

    /**
     * @brief Deduction guides required for arguments to span, CTAD will fail for iterators, arrays and containers without this
     */
    template<class It, class End, size_t Extent = std::dynamic_extent>
    span(It, End) -> span<typename std::iterator_traits<It>::value_type, Extent>;
    template<class T, size_t Size>
    span(T (&)[Size]) -> span<T, Size>;
    template<class T, size_t Size>
    span(std::array<T, Size> &) -> span<T, Size>;
    template<class T, size_t Size>
    span(const std::array<T, Size> &) -> span<const T, Size>;
    template<class Container>
    span(Container &) -> span<typename Container::value_type>;
    template<class Container>
    span(const Container &) -> span<const typename Container::value_type>;

    /**
     * @brief The Logger class is to write log output to file and logcat
     */
    class Logger {
      private:
        std::ofstream logFile; //!< An output stream to the log file
        std::mutex mtx; //!< A mutex to lock before logging anything

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

        /**
         * @brief Writes a header, should only be used for emulation starting and ending
         */
        void WriteHeader(const std::string &str);

        void Write(LogLevel level, std::string str);

        template<typename S, typename... Args>
        inline void Error(const S &formatStr, Args &&... args) {
            if (LogLevel::Error <= configLevel) {
                Write(LogLevel::Error, fmt::format(formatStr, util::FmtCast(args)...));
            }
        }

        template<typename S, typename... Args>
        inline void Warn(const S &formatStr, Args &&... args) {
            if (LogLevel::Warn <= configLevel) {
                Write(LogLevel::Warn, fmt::format(formatStr, util::FmtCast(args)...));
            }
        }

        template<typename S, typename... Args>
        inline void Info(const S &formatStr, Args &&... args) {
            if (LogLevel::Info <= configLevel) {
                Write(LogLevel::Info, fmt::format(formatStr, util::FmtCast(args)...));
            }
        }

        template<typename S, typename... Args>
        inline void Debug(const S &formatStr, Args &&... args) {
            if (LogLevel::Debug <= configLevel) {
                Write(LogLevel::Debug, fmt::format(formatStr, util::FmtCast(args)...));
            }
        }

        template<typename S, typename... Args>
        inline void Verbose(const S &formatStr, Args &&... args) {
            if (LogLevel::Verbose <= configLevel) {
                Write(LogLevel::Verbose, fmt::format(formatStr, util::FmtCast(args)...));
            }
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
    namespace kernel {
        namespace type {
            class KProcess;
            class KThread;
        }
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

        kernel::OS *os;
        std::shared_ptr<JvmManager> jvm;
        std::shared_ptr<Settings> settings;
        std::shared_ptr<Logger> logger;
        std::shared_ptr<loader::Loader> loader;
        std::shared_ptr<gpu::GPU> gpu;
        std::shared_ptr<audio::Audio> audio;
        std::shared_ptr<input::Input> input;
        std::shared_ptr<nce::NCE> nce;
        std::shared_ptr<kernel::type::KProcess> process;
        thread_local static std::shared_ptr<kernel::type::KThread> thread; //!< The KThread of the thread which accesses this object
        thread_local static nce::ThreadContext *ctx; //!< The context of the guest thread for the corresponding host thread
    };
}
