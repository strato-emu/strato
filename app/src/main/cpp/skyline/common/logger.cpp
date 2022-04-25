// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/log.h>
#include "utils.h"
#include "logger.h"

namespace skyline {
    void Logger::LoggerContext::Initialize(const std::string &path) {
        start = util::GetTimeNs() / constant::NsInMillisecond;
        logFile.open(path, std::ios::trunc);
    }

    void Logger::LoggerContext::Finalize() {
        std::scoped_lock lock{mutex};
        logFile.close();
    }

    void Logger::LoggerContext::TryFlush() {
        std::unique_lock lock(mutex, std::try_to_lock);
        if (lock)
            logFile.flush();
    }

    void Logger::LoggerContext::Flush() {
        std::scoped_lock lock{mutex};
        logFile.flush();
    }

    thread_local static std::string logTag, threadName;
    thread_local static Logger::LoggerContext *context{&Logger::EmulationContext};

    void Logger::UpdateTag() {
        std::array<char, 16> name;
        if (!pthread_getname_np(pthread_self(), name.data(), name.size()))
            threadName = name.data();
        else
            threadName = "unk";
        logTag = std::string("emu-cpp-") + threadName;
    }

    Logger::LoggerContext *Logger::GetContext() {
        return context;
    }

    void Logger::SetContext(LoggerContext *pContext) {
        context = pContext;
    }

    void Logger::WriteAndroid(LogLevel level, const std::string &str) {
        constexpr std::array<int, 5> levelAlog{ANDROID_LOG_ERROR, ANDROID_LOG_WARN, ANDROID_LOG_INFO, ANDROID_LOG_DEBUG, ANDROID_LOG_VERBOSE}; // This corresponds to LogLevel and provides its equivalent for NDK Logging
        if (logTag.empty())
            UpdateTag();

        __android_log_write(levelAlog[static_cast<u8>(level)], logTag.c_str(), str.c_str());
    }

    void Logger::Write(LogLevel level, const std::string &str) {
        constexpr std::array<char, 5> levelCharacter{'E', 'W', 'I', 'D', 'V'}; // The LogLevel as written out to a file
        WriteAndroid(level, str);

        if (context)
            // We use RS (\036) and GS (\035) as our delimiters
            context->Write(fmt::format("\036{}\035{}\035{}\035{}\n", levelCharacter[static_cast<u8>(level)], (util::GetTimeNs() / constant::NsInMillisecond) - context->start, threadName, str));
    }

    void Logger::LoggerContext::Write(const std::string &str) {
        std::scoped_lock guard{mutex};
        logFile << str;
    }
}
