// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <bit>
#include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <thread>
#include <string>
#include <memory>
#include <compare>
#include <boost/container/small_vector.hpp>
#include <common/span.h>
#include <common/result.h>

namespace skyline {
    /**
     * @brief A wrapper around writing logs into a log file and logcat using Android Log APIs
     */
    class Logger {
      private:
        std::mutex mutex; //!< Synchronizes all output I/O to ensure there are no races
        std::ofstream logFile; //!< An output stream to the log file
        i64 start; //!< A timestamp in milliseconds for when the logger was started, this is used as the base for all log timestamps

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

            constexpr FunctionString(S string, const char *function = __builtin_FUNCTION()) : string(std::move(string)), function(function) {}

            std::string operator*() {
                return std::string(function) + ": " + std::string(string);
            }
        };

        template<typename... Args>
        void Error(FunctionString<const char*> formatString, Args &&... args) {
            if (LogLevel::Error <= configLevel)
                Write(LogLevel::Error, util::Format(*formatString, args...));
        }

        template<typename... Args>
        void Error(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Error <= configLevel)
                Write(LogLevel::Error, util::Format(*formatString, args...));
        }

        template<typename S, typename... Args>
        void ErrorNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Error <= configLevel)
                Write(LogLevel::Error, util::Format(formatString, args...));
        }

        template<typename... Args>
        void Warn(FunctionString<const char*> formatString, Args &&... args) {
            if (LogLevel::Warn <= configLevel)
                Write(LogLevel::Warn, util::Format(*formatString, args...));
        }

        template<typename... Args>
        void Warn(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Warn <= configLevel)
                Write(LogLevel::Warn, util::Format(*formatString, args...));
        }

        template<typename S, typename... Args>
        void WarnNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Warn <= configLevel)
                Write(LogLevel::Warn, util::Format(formatString, args...));
        }

        template<typename... Args>
        void Info(FunctionString<const char*> formatString, Args &&... args) {
            if (LogLevel::Info <= configLevel)
                Write(LogLevel::Info, util::Format(*formatString, args...));
        }

        template<typename... Args>
        void Info(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Info <= configLevel)
                Write(LogLevel::Info, util::Format(*formatString, args...));
        }

        template<typename S, typename... Args>
        void InfoNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Info <= configLevel)
                Write(LogLevel::Info, util::Format(formatString, args...));
        }

        template<typename... Args>
        void Debug(FunctionString<const char*> formatString, Args &&... args) {
            if (LogLevel::Debug <= configLevel)
                Write(LogLevel::Debug, util::Format(*formatString, args...));
        }

        template<typename... Args>
        void Debug(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Debug <= configLevel)
                Write(LogLevel::Debug, util::Format(*formatString, args...));
        }

        template<typename S, typename... Args>
        void DebugNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Debug <= configLevel)
                Write(LogLevel::Debug, util::Format(formatString, args...));
        }

        template<typename... Args>
        void Verbose(FunctionString<const char*> formatString, Args &&... args) {
            if (LogLevel::Verbose <= configLevel)
                Write(LogLevel::Verbose, util::Format(*formatString, args...));
        }

        template<typename... Args>
        void Verbose(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Verbose <= configLevel)
                Write(LogLevel::Verbose, util::Format(*formatString, args...));
        }

        template<typename S, typename... Args>
        void VerboseNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Verbose <= configLevel)
                Write(LogLevel::Verbose, util::Format(formatString, args...));
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
