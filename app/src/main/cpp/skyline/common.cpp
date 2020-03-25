#include "common.h"
#include "nce.h"
#include "gpu.h"
#include "audio.h"
#include <kernel/types/KThread.h>
#include <tinyxml2.h>

namespace skyline {
    void Mutex::lock() {
        while (true) {
            for (int i = 0; i < 1000; ++i) {
                if (!flag.test_and_set(std::memory_order_acquire))
                    return;

                asm volatile("yield");
            }
            sched_yield();
        }
    }

    void GroupMutex::lock(Group group) {
        auto none = Group::None;
        constexpr u64 timeout = 100; // The timeout in ns
        auto end = utils::GetTimeNs() + timeout;

        while (true) {
            if (next == group) {
                if (flag == group) {
                    std::lock_guard lock(mtx);

                    if (flag == group) {
                        auto groupT = group;
                        next.compare_exchange_strong(groupT, Group::None);
                        num++;

                        return;
                    }
                } else {
                    flag.compare_exchange_weak(none, group);
                }
            } else if (flag == group && (next == Group::None || utils::GetTimeNs() >= end)) {
                std::lock_guard lock(mtx);

                if (flag == group) {
                    num++;
                    return;
                }
            } else {
                next.compare_exchange_weak(none, group);
            }

            none = Group::None;
            asm volatile("yield");
        }
    }

    void GroupMutex::unlock() {
        std::lock_guard lock(mtx);

        if (!--num)
            flag.exchange(next);
    }

    Settings::Settings(const int preferenceFd) {
        tinyxml2::XMLDocument pref;

        if (pref.LoadFile(fdopen(preferenceFd, "r")))
            throw exception("TinyXML2 Error: " + std::string(pref.ErrorStr()));

        tinyxml2::XMLElement *elem = pref.LastChild()->FirstChild()->ToElement();

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
                    syslog(LOG_ALERT, "Settings type is missing: %s for %s", elem->Value(), elem->FindAttribute("name")->Value());
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

    Logger::Logger(const int logFd, LogLevel configLevel) : configLevel(configLevel) {
        logFile.__open(logFd, std::ios::app);
        WriteHeader("Logging started");
    }

    Logger::~Logger() {
        WriteHeader("Logging ended");
    }

    void Logger::WriteHeader(const std::string &str) {
        syslog(LOG_ALERT, "%s", str.c_str());

        logFile << "0|" << str << "\n";
        logFile.flush();
    }

    void Logger::Write(const LogLevel level, std::string str) {
        syslog(levelSyslog[static_cast<u8>(level)], "%s", str.c_str());

        for (auto &character : str)
            if (character == '\n')
                character = '\\';

        logFile << "1|" << levelStr[static_cast<u8>(level)] << "|" << str << "\n";
        logFile.flush();
    }

    DeviceState::DeviceState(kernel::OS *os, std::shared_ptr<kernel::type::KProcess> &process, std::shared_ptr<JvmManager> jvmManager, std::shared_ptr<Settings> settings, std::shared_ptr<Logger> logger)
        : os(os), jvmManager(std::move(jvmManager)), settings(std::move(settings)), logger(std::move(logger)), process(process) {
        // We assign these later as they use the state in their constructor and we don't want null pointers
        nce = std::move(std::make_shared<NCE>(*this));
        gpu = std::move(std::make_shared<gpu::GPU>(*this));
        audio = std::move(std::make_shared<audio::Audio>(*this));
    }

    thread_local std::shared_ptr<kernel::type::KThread> DeviceState::thread = nullptr;
    thread_local ThreadContext *DeviceState::ctx = nullptr;
}
