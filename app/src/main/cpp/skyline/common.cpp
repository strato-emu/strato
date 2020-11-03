// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/log.h>
#include "common.h"
#include "nce.h"
#include "gpu.h"
#include "audio.h"
#include "input.h"
#include "kernel/types/KThread.h"

namespace skyline {
    Logger::Logger(const std::string &path, LogLevel configLevel) : configLevel(configLevel) {
        logFile.open(path, std::ios::trunc);
        UpdateTag();
        WriteHeader("Logging started");
    }

    Logger::~Logger() {
        WriteHeader("Logging ended");
        logFile.flush();
    }

    thread_local static std::string logTag, threadName;

    void Logger::UpdateTag() {
        std::array<char, 16> name;
        if (!pthread_getname_np(pthread_self(), name.data(), name.size()))
             threadName = name.data();
        else
            threadName = "unk";
        logTag = std::string("emu-cpp-") + threadName;
    }

    void Logger::WriteHeader(const std::string &str) {
        __android_log_write(ANDROID_LOG_INFO, "emu-cpp", str.c_str());

        std::lock_guard guard(mtx);
        logFile << "0|" << str << "\n";
        logFile.flush();
    }

    void Logger::Write(LogLevel level, std::string str) {
        constexpr std::array<char, 5> levelCharacter{'0', '1', '2', '3', '4'}; // The LogLevel as written out to a file
        constexpr std::array<int, 5> levelAlog{ANDROID_LOG_ERROR, ANDROID_LOG_WARN, ANDROID_LOG_INFO, ANDROID_LOG_DEBUG, ANDROID_LOG_VERBOSE}; // This corresponds to LogLevel and provides it's equivalent for NDK Logging

        if (logTag.empty())
            UpdateTag();

        __android_log_write(levelAlog[static_cast<u8>(level)], logTag.c_str(), str.c_str());

        for (auto &character : str)
            if (character == '\n')
                character = '\\';

        std::lock_guard guard(mtx);
        logFile << "1|" << levelCharacter[static_cast<u8>(level)] << '|' << threadName << '|' << str << '\n';
        logFile.flush();
    }

    DeviceState::DeviceState(kernel::OS *os, std::shared_ptr<JvmManager> jvmManager, std::shared_ptr<Settings> settings, std::shared_ptr<Logger> logger)
        : os(os), jvm(std::move(jvmManager)), settings(std::move(settings)), logger(std::move(logger)) {
        // We assign these later as they use the state in their constructor and we don't want null pointers
        nce = std::make_shared<nce::NCE>(*this);
        gpu = std::make_shared<gpu::GPU>(*this);
        audio = std::make_shared<audio::Audio>(*this);
        input = std::make_shared<input::Input>(*this);
    }

    thread_local std::shared_ptr<kernel::type::KThread> DeviceState::thread = nullptr;
    thread_local nce::ThreadContext *DeviceState::ctx = nullptr;
}
