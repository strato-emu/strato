// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <map>
#include <unordered_map>
#include <span>
#include <vector>
#include <fstream>
#include <syslog.h>
#include <mutex>
#include <thread>
#include <string>
#include <sstream>
#include <memory>
#include <fmt/format.h>
#include <sys/mman.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <jni.h>
#include "nce/guest_common.h"

namespace skyline {
    using KHandle = u32; //!< The type of a kernel handle

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
        // Memory
        constexpr u64 BaseAddress = 0x8000000; //!< The address space base
        constexpr u64 DefStackSize = 0x1E8480; //!< The default amount of stack: 2 MB
        // Display
        constexpr u16 HandheldResolutionW = 1280; //!< The width component of the handheld resolution
        constexpr u16 HandheldResolutionH = 720; //!< The height component of the handheld resolution
        constexpr u16 DockedResolutionW = 1920; //!< The width component of the docked resolution
        constexpr u16 DockedResolutionH = 1080; //!< The height component of the docked resolution
        // Time
        constexpr u64 NsInSecond = 1000000000; //!< This is the amount of nanoseconds in a second
    };

    namespace util {
        /**
         * @brief Returns the current time in nanoseconds
         * @return The current time in nanoseconds
         */
        inline u64 GetTimeNs() {
            static u64 frequency{};
            if (!frequency)
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
         * @brief Aligns up a value to a multiple of two
         * @tparam Type The type of the values
         * @param value The value to round up
         * @param multiple The multiple to round up to (Should be a multiple of 2)
         * @tparam TypeVal The type of the value
         * @tparam TypeMul The type of the multiple
         * @return The aligned value
         */
        template<typename TypeVal, typename TypeMul>
        constexpr inline TypeVal AlignUp(TypeVal value, TypeMul multiple) {
            static_assert(std::is_integral<TypeVal>() && std::is_integral<TypeMul>());
            multiple--;
            return (value + multiple) & ~(multiple);
        }

        /**
         * @brief Aligns down a value to a multiple of two
         * @param value The value to round down
         * @param multiple The multiple to round down to (Should be a multiple of 2)
         * @tparam TypeVal The type of the value
         * @tparam TypeMul The type of the multiple
         * @return The aligned value
         */
        template<typename TypeVal, typename TypeMul>
        constexpr inline TypeVal AlignDown(TypeVal value, TypeMul multiple) {
            static_assert(std::is_integral<TypeVal>() && std::is_integral<TypeMul>());
            return value & ~(multiple - 1);
        }

        /**
         * @param value The value to check for alignment
         * @param multiple The multiple to check alignment with
         * @return If the address is aligned with the multiple
         */
        constexpr inline bool IsAligned(u64 value, u64 multiple) {
            return !(value & (multiple - 1U));
        }

        /**
         * @param value The value to check for alignment
         * @return If the value is page aligned
         */
        constexpr inline bool PageAligned(u64 value) {
            return IsAligned(value, PAGE_SIZE);
        }

        /**
         * @param value The value to check for alignment
         * @return If the value is word aligned
         */
        constexpr inline bool WordAligned(u64 value) {
            return IsAligned(value, WORD_BIT / 8);
        }

        /**
         * @param string The string to create a magic from
         * @return The magic of the supplied string
         */
        template<typename Type>
        constexpr Type MakeMagic(std::string_view string) {
            Type object{};
            auto offset = 0;

            for (auto &character : string) {
                object |= static_cast<Type>(character) << offset;
                offset += sizeof(character) * 8;
            }

            return object;
        }
    }

    /**
     * @brief The Mutex class is a wrapper around an atomic bool used for synchronization
     */
    class Mutex {
        std::atomic_flag flag = ATOMIC_FLAG_INIT; //!< An atomic flag to hold the state of the mutex

      public:
        /**
         * @brief Wait on and lock the mutex
         */
        void lock();

        /**
         * @brief Try to lock the mutex if it is unlocked else return
         * @return If the mutex was successfully locked or not
         */
        inline bool try_lock() {
            return !flag.test_and_set(std::memory_order_acquire);
        }

        /**
         * @brief Unlock the mutex if it is held by this thread
         */
        inline void unlock() {
            flag.clear(std::memory_order_release);
        }
    };

    /**
     * @brief The GroupMutex class is a special type of mutex that allows two groups of users and only allows one group to run in parallel
     */
    class GroupMutex {
      public:
        /**
         * @brief This enumeration holds all the possible owners of the mutex
         */
        enum class Group : u8 {
            None = 0, //!< No group owns this mutex
            Group1 = 1, //!< Group 1 owns this mutex
            Group2 = 2 //!< Group 2 owns this mutex
        };

        /**
         * @brief Wait on and lock the mutex
         */
        void lock(Group group = Group::Group1);

        /**
         * @brief Unlock the mutex
         * @note Undefined behavior in case unlocked by thread in non-owner group
         */
        void unlock();

      private:
        std::atomic<Group> flag{Group::None}; //!< An atomic flag to hold which group holds the mutex
        std::atomic<Group> next{Group::None}; //!< An atomic flag to hold which group will hold the mutex next
        std::atomic<u8> num{0}; //!< An atomic u8 keeping track of how many users are holding the mutex
        Mutex mtx; //!< A mutex to lock before changing of num and flag
    };

    /**
     * @brief The Logger class is to write log output to file and logcat
     */
    class Logger {
      private:
        std::ofstream logFile; //!< An output stream to the log file
        const char *levelStr[4] = {"0", "1", "2", "3"}; //!< This is used to denote the LogLevel when written out to a file
        static constexpr int levelSyslog[4] = {LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG}; //!< This corresponds to LogLevel and provides it's equivalent for syslog
        Mutex mtx; //!< A mutex to lock before logging anything

      public:
        enum class LogLevel { Error, Warn, Info, Debug }; //!< The level of a particular log
        LogLevel configLevel; //!< The level of logs to write

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
         * @brief Writes a header, should only be used for emulation starting and ending
         * @param str The value to be written
         */
        void WriteHeader(const std::string &str);

        /**
         * @brief Write a log to the log file
         * @param level The level of the log
         * @param str The value to be written
         */
        void Write(LogLevel level, std::string str);

        /**
         * @brief Write an error log with libfmt formatting
         * @param formatStr The value to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        inline void Error(const S &formatStr, Args &&... args) {
            if (LogLevel::Error <= configLevel) {
                Write(LogLevel::Error, fmt::format(formatStr, args...));
            }
        }

        /**
         * @brief Write a debug log with libfmt formatting
         * @param formatStr The value to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        inline void Warn(const S &formatStr, Args &&... args) {
            if (LogLevel::Warn <= configLevel) {
                Write(LogLevel::Warn, fmt::format(formatStr, args...));
            }
        }

        /**
         * @brief Write a debug log with libfmt formatting
         * @param formatStr The value to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        inline void Info(const S &formatStr, Args &&... args) {
            if (LogLevel::Info <= configLevel) {
                Write(LogLevel::Info, fmt::format(formatStr, args...));
            }
        }

        /**
         * @brief Write a debug log with libfmt formatting
         * @param formatStr The value to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        inline void Debug(const S &formatStr, Args &&... args) {
            if (LogLevel::Debug <= configLevel) {
                Write(LogLevel::Debug, fmt::format(formatStr, args...));
            }
        }
    };

    /**
     * @brief The Settings class is used to access the parameters set in the Java component of the application
     */
    class Settings {
      private:
        std::map<std::string, std::string> stringMap; //!< A mapping from all keys to their corresponding string value
        std::map<std::string, bool> boolMap; //!< A mapping from all keys to their corresponding boolean value
        std::map<std::string, int> intMap; //!< A mapping from all keys to their corresponding integer value

      public:
        /**
         * @param fd An FD to the preference XML file
         */
        Settings(int fd);

        /**
         * @brief Retrieves a particular setting as a string
         * @param key The key of the setting
         * @return The string value of the setting
         */
        std::string GetString(const std::string &key);

        /**
         * @brief Retrieves a particular setting as a boolean
         * @param key The key of the setting
         * @return The boolean value of the setting
         */
        bool GetBool(const std::string &key);

        /**
         * @brief Retrieves a particular setting as a integer
         * @param key The key of the setting
         * @return The integer value of the setting
         */
        int GetInt(const std::string &key);

        /**
         * @brief Writes all settings keys and values to syslog. This function is for development purposes.
         */
        void List(const std::shared_ptr<Logger> &logger);
    };

    /**
     * @brief This is a std::runtime_error with libfmt formatting
     */
    class exception : public std::runtime_error {
      public:
        /**
         * @param formatStr The exception string to be written, with libfmt formatting
         * @param args The arguments based on format_str
         */
        template<typename S, typename... Args>
        inline exception(const S &formatStr, Args &&... args) : runtime_error(fmt::format(formatStr, args...)) {}
    };

    class NCE;
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
     * @brief This struct is used to hold the state of a device
     */
    struct DeviceState {
        DeviceState(kernel::OS *os, std::shared_ptr<kernel::type::KProcess> &process, std::shared_ptr<JvmManager> jvmManager, std::shared_ptr<Settings> settings, std::shared_ptr<Logger> logger);

        kernel::OS *os; //!< This holds a reference to the OS class
        std::shared_ptr<kernel::type::KProcess> &process; //!< This holds a reference to the process object
        thread_local static std::shared_ptr<kernel::type::KThread> thread; //!< This holds a reference to the current thread object
        thread_local static ThreadContext *ctx; //!< This holds the context of the thread
        std::shared_ptr<NCE> nce; //!< This holds a reference to the NCE class
        std::shared_ptr<gpu::GPU> gpu; //!< This holds a reference to the GPU class
        std::shared_ptr<audio::Audio> audio; //!< This holds a reference to the Audio class
        std::shared_ptr<input::Input> input; //!< This holds a reference to the Input class
        std::shared_ptr<loader::Loader> loader; //!< This holds a reference to the Loader class
        std::shared_ptr<JvmManager> jvm; //!< This holds a reference to the JvmManager class
        std::shared_ptr<Settings> settings; //!< This holds a reference to the Settings class
        std::shared_ptr<Logger> logger; //!< This holds a reference to the Logger class
    };
}
