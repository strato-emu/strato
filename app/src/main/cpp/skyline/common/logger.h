// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <fstream>
#include <mutex>
#include "base.h"

namespace skyline {
    /**
     * @brief A wrapper around writing logs into a log file and logcat using Android Log APIs
     */
    class Logger {
      private:
        Logger() {}

      public:
        enum class LogLevel {
            Error,
            Warn,
            Info,
            Debug,
            Verbose,
        };

        static inline LogLevel configLevel{LogLevel::Verbose}; //!< The minimum level of logs to write

        /**
         * @brief Holds logger variables that cannot be static
         */
        struct LoggerContext {
            std::mutex mutex; //!< Synchronizes all output I/O to ensure there are no races
            std::ofstream logFile; //!< An output stream to the log file
            i64 start; //!< A timestamp in milliseconds for when the logger was started, this is used as the base for all log timestamps

            LoggerContext() {}

            void Initialize(const std::string &path);

            void Finalize();

            void TryFlush();

            void Flush();

            void Write(const std::string &str);
        };
        static inline LoggerContext EmulationContext, LoaderContext;

        /**
         * @brief Update the tag in log messages with a new thread name
         */
        static void UpdateTag();

        static LoggerContext *GetContext();

        static void SetContext(LoggerContext *context);

        static void WriteAndroid(LogLevel level, const std::string &str);

        static void Write(LogLevel level, const std::string &str);

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
        static void Error(FunctionString<const char *> formatString, Args &&... args) {
            if (LogLevel::Error <= configLevel)
                Write(LogLevel::Error, util::Format(*formatString, args...));
        }

        template<typename... Args>
        static void Error(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Error <= configLevel)
                Write(LogLevel::Error, util::Format(*formatString, args...));
        }

        template<typename S, typename... Args>
        static void ErrorNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Error <= configLevel)
                Write(LogLevel::Error, util::Format(formatString, args...));
        }

        template<typename... Args>
        static void Warn(FunctionString<const char *> formatString, Args &&... args) {
            if (LogLevel::Warn <= configLevel)
                Write(LogLevel::Warn, util::Format(*formatString, args...));
        }

        template<typename... Args>
        static void Warn(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Warn <= configLevel)
                Write(LogLevel::Warn, util::Format(*formatString, args...));
        }

        template<typename S, typename... Args>
        static void WarnNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Warn <= configLevel)
                Write(LogLevel::Warn, util::Format(formatString, args...));
        }

        template<typename... Args>
        static void Info(FunctionString<const char *> formatString, Args &&... args) {
            if (LogLevel::Info <= configLevel)
                Write(LogLevel::Info, util::Format(*formatString, args...));
        }

        template<typename... Args>
        static void Info(FunctionString<std::string> formatString, Args &&... args) {
            if (LogLevel::Info <= configLevel)
                Write(LogLevel::Info, util::Format(*formatString, args...));
        }

        template<typename S, typename... Args>
        static void InfoNoPrefix(S formatString, Args &&... args) {
            if (LogLevel::Info <= configLevel)
                Write(LogLevel::Info, util::Format(formatString, args...));
        }

        template<typename... Args>
        static void Debug(FunctionString<const char *> formatString, Args &&... args) {
            #ifndef NDEBUG
            if (LogLevel::Debug <= configLevel)
                Write(LogLevel::Debug, util::Format(*formatString, args...));
            #endif
        }

        template<typename... Args>
        static void Debug(FunctionString<std::string> formatString, Args &&... args) {
            #ifndef NDEBUG
            if (LogLevel::Debug <= configLevel)
                Write(LogLevel::Debug, util::Format(*formatString, args...));
            #endif
        }

        template<typename S, typename... Args>
        static void DebugNoPrefix(S formatString, Args &&... args) {
            #ifndef NDEBUG
            if (LogLevel::Debug <= configLevel)
                Write(LogLevel::Debug, util::Format(formatString, args...));
            #endif
        }

        template<typename... Args>
        static void Verbose(FunctionString<const char *> formatString, Args &&... args) {
            #ifndef NDEBUG
            if (LogLevel::Verbose <= configLevel)
                Write(LogLevel::Verbose, util::Format(*formatString, args...));
            #endif
        }

        template<typename... Args>
        static void Verbose(FunctionString<std::string> formatString, Args &&... args) {
            #ifndef NDEBUG
            if (LogLevel::Verbose <= configLevel)
                Write(LogLevel::Verbose, util::Format(*formatString, args...));
            #endif
        }

        template<typename S, typename... Args>
        static void VerboseNoPrefix(S formatString, Args &&... args) {
            #ifndef NDEBUG
            if (LogLevel::Verbose <= configLevel)
                Write(LogLevel::Verbose, util::Format(formatString, args...));
            #endif
        }
    };
}
