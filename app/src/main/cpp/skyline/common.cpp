// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <tinyxml2.h>
#include <android/log.h>
#include "common.h"
#include "nce.h"
#include "gpu.h"
#include "audio.h"
#include "input.h"
#include "kernel/types/KThread.h"

namespace skyline {
    Settings::Settings(int fd) {
        tinyxml2::XMLDocument pref;

        auto fileDeleter = [](FILE *file) { fclose(file); };
        std::unique_ptr<FILE, decltype(fileDeleter)> file{fdopen(fd, "r"), fileDeleter};
        if (pref.LoadFile(file.get()))
            throw exception("TinyXML2 Error: " + std::string(pref.ErrorStr()));

        tinyxml2::XMLElement *elem{pref.LastChild()->FirstChild()->ToElement()};
        while (elem) {
            switch (elem->Value()[0]) {
                case 's':
                    stringMap[elem->FindAttribute("name")->Value()] = elem->GetText();
                    break;

                case 'b':
                    boolMap[elem->FindAttribute("name")->Value()] = elem->FindAttribute("value")->BoolValue();
                    break;

                case 'i':
                    intMap[elem->FindAttribute("name")->Value()] = elem->FindAttribute("value")->IntValue();
                    break;

                default:
                    __android_log_print(ANDROID_LOG_WARN, "emu-cpp", "Settings type is missing: %s for %s", elem->Value(), elem->FindAttribute("name")->Value());
                    break;
            };

            if (elem->NextSibling())
                elem = elem->NextSibling()->ToElement();
            else
                break;
        }

        pref.Clear();
    }

    std::string Settings::GetString(const std::string &key) {
        return stringMap.at(key);
    }

    bool Settings::GetBool(const std::string &key) {
        return boolMap.at(key);
    }

    int Settings::GetInt(const std::string &key) {
        return intMap.at(key);
    }

    void Settings::List(const std::shared_ptr<Logger> &logger) {
        for (auto &iter : stringMap)
            logger->Info("Key: {}, Value: {}, Type: String", iter.first, GetString(iter.first));

        for (auto &iter : boolMap)
            logger->Info("Key: {}, Value: {}, Type: Bool", iter.first, GetBool(iter.first));
    }

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
    }

    DeviceState::DeviceState(kernel::OS *os, std::shared_ptr<kernel::type::KProcess> &process, std::shared_ptr<JvmManager> jvmManager, std::shared_ptr<Settings> settings, std::shared_ptr<Logger> logger)
        : os(os), jvm(std::move(jvmManager)), settings(std::move(settings)), logger(std::move(logger)), process(process) {
        // We assign these later as they use the state in their constructor and we don't want null pointers
        nce = std::make_shared<nce::NCE>(*this);
        gpu = std::make_shared<gpu::GPU>(*this);
        audio = std::make_shared<audio::Audio>(*this);
        input = std::make_shared<input::Input>(*this);
    }

    thread_local std::shared_ptr<kernel::type::KThread> DeviceState::thread = nullptr;
    thread_local nce::ThreadContext *DeviceState::ctx = nullptr;
}
