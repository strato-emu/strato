// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#include <atomic>
#include <chrono>
#include <mutex>
#include <fstream>
#include <pthread.h>
#include <thread>
#include <common/base.h>
#include <common/circular_queue.h>
#include <android/log.h>
#include <common/format.h>
#include <fmt/compile.h>
#include "logger.h"

namespace skyline {
    constexpr size_t PrefixLength{8}; //!< The length of the prefix "emu-cpp-"
    constexpr size_t MaxThreadNameLength{16}; //!< The maximum length of a thread name

    /**
     * @brief The logging context for the current thread
     */
    thread_local struct ThreadLogContext {
        /**
         * @brief The shared storage for the thread name and log tag
         */
        std::array<char, PrefixLength + MaxThreadNameLength> threadNameStorage{"emu-cpp-unk"};

        /**
         * @returns A span of the thread name
         */
        constexpr std::span<char> GetThreadName() {
            return {threadNameStorage.begin() + PrefixLength, threadNameStorage.end()};
        }

        /**
         * @return A span of the log tag
         */
        constexpr std::span<char> GetLogTag() {
            return {threadNameStorage.begin(), threadNameStorage.end()};
        }
    } threadContext;

    using LogLevel = AsyncLogger::LogLevel;

    using clock = std::chrono::steady_clock;
    using microseconds = std::chrono::microseconds;
    using log_time_point = std::chrono::time_point<clock>;

    struct LogMessage {
        LogLevel level; //!< The level of the log message
        const char *function; //!< The name of the function that pushed this message
        std::string str; //!< The log message string
        log_time_point time; //!< The time when the message was pushed
        ThreadLogContext *threadContext; //!< The context for the thread that pushed this message

        LogMessage(LogLevel level, const char *function, std::string &&str, log_time_point time, ThreadLogContext *threadContext)
            : level(level), function(function), str(std::move(str)), time(time), threadContext(threadContext) {}

        // Disable copy constructor to prevent copying the string
        LogMessage(const LogMessage &) = delete;

        // Disable copy assignment to prevent copying the string
        LogMessage &operator=(const LogMessage &) = delete;

        LogMessage(LogMessage &&) = default;

        LogMessage &operator=(LogMessage &&) = default;
    };

    /**
     * @brief The implementation class of the logger, holds instance data and the writer thread
     */
    class AsyncLogger::Impl {
        std::mutex fileMutex; //!< Synchronizes write operations to the log file
        std::ofstream logFile; //!< An output stream to the log file
        log_time_point start; //!< A time point when the logger was started, used as the base for all log timestamps
        std::atomic<log_time_point> stop; //!< A time point when the logger was stopped, used by the writer thread to know when to stop

        LogLevel currentLevel; //!< The minimum level of logs to write
        CircularQueue<LogMessage> messageQueue{AsyncLogger::LogQueueSize}; //!< The queue where all log messages are sent

        std::atomic<bool> running{true}; //!< If the logger thread should continue running
        std::thread thread; //!< The thread that handles writing log entries from the logger queue

      public:
        /**
         * @brief Constructs an empty instance that does not write any logs
         */
        Impl() noexcept : currentLevel(LogLevel::Disabled) {}

        /**
         * @brief Constructs an instance that writes logs to the file at the given path
         */
        Impl(AsyncLogger::LogLevel level, const std::filesystem::path &path) : currentLevel(level) {
            start = clock::now();
            std::filesystem::create_directories(path.parent_path());
            logFile.open(path, std::ios::trunc);
            thread = std::thread{&Impl::WriterThread, this};
        }

        /**
         * @brief Finalizes the logger and flushes all pending logs
         * @param wait If the function should wait for the writer thread to exit
         */
        void Finalize(bool wait = false) {
            stop.store(clock::now(), std::memory_order_release);
            running.store(false, std::memory_order_release);
            if (wait)
                thread.join();

            std::scoped_lock lock{fileMutex};
            logFile.close();
        }

        /**
         * @brief The function that runs on the writer thread, polls the queue for new messages and writes them out
         */
        void WriterThread() {
            pthread_setname_np(pthread_self(), "Sky-Logger");

            while (running.load(std::memory_order_acquire)) {
                auto message{messageQueue.Pop()};
                Write(message);
            }

            log_time_point stopTime = stop.load(std::memory_order_acquire);
            // Write out remaining messages in the queue up to the stop time
            while (!messageQueue.Empty()) {
                auto message{messageQueue.Pop()};
                Write(message);

                if (message.time > stopTime)
                    break;
            }
        }

        /**
         * @brief Pushes a log message into the queue
         */
        void Push(LogMessage &&message) {
            messageQueue.Push(std::move(message));
        }

        /**
         * @return Whether the message with the given log level should be written
         */
        bool CheckLogLevel(AsyncLogger::LogLevel level) {
            return level >= currentLevel;
        }

        /**
         * @brief Writes a message to the log file and logcat
         */
        void Write(LogMessage &message) {
            // Prefix the function name to the message
            if (message.function)
                message.str = fmt::format(FMT_COMPILE("{}: {}"), message.function, message.str);

            WriteAndroid(message);
            WriteFile(message);
        }

      private:
        void WriteAndroid(const LogMessage &message) {
            constexpr std::array<int, 5> androidLevel{
                ANDROID_LOG_VERBOSE,
                ANDROID_LOG_DEBUG,
                ANDROID_LOG_INFO,
                ANDROID_LOG_WARN,
                ANDROID_LOG_ERROR,
            }; // The LogLevel as Android NDK log level

            __android_log_write(androidLevel[static_cast<u32>(message.level)], message.threadContext->GetLogTag().data(), message.str.c_str());
        }

        void WriteFile(const LogMessage &message) {
            constexpr std::array<const char *, 5> levelTag{
                "VERBOSE",
                "DEBUG",
                "INFO",
                "WARNING",
                "ERROR",
            };

            std::scoped_lock lock{fileMutex};

            // LEVEL__ | ______TIME | ____THREAD_____ | MESSAGE
            logFile << fmt::format(
                "{:7} | {:>10} | {:^15} | {}\n",
                levelTag[static_cast<u32>(message.level)],
                duration_cast<microseconds>(message.time - start).count(),
                message.threadContext->GetThreadName().data(),
                message.str
            );
            logFile.flush();
        }
    };

    AsyncLogger::Impl impl{};

    void AsyncLogger::Initialize(LogLevel level, const std::filesystem::path &path) {
        std::construct_at(&impl, level, path);
        UpdateTag(); // Update the tag for the calling thread
    }

    void AsyncLogger::Finalize(bool wait) {
        impl.Finalize(wait);
        std::destroy_at(&impl);
    }

    void AsyncLogger::UpdateTag() {
        pthread_getname_np(pthread_self(), threadContext.GetThreadName().data(), MaxThreadNameLength);
    }

    bool AsyncLogger::CheckLogLevel(LogLevel level) {
        return impl.CheckLogLevel(level);
    }

    void AsyncLogger::LogAsync(LogLevel level, std::string &&str, const char *function) {
        impl.Push(LogMessage{
            level,
            function,
            std::move(str),
            clock::now(),
            &threadContext
        });
    }

    void AsyncLogger::LogSync(LogLevel level, std::string &&str, const char *function) {
        auto message = LogMessage{
            level,
            function,
            std::move(str),
            clock::now(),
            &threadContext
        };
        impl.Write(message);
    }
}
