#include "common.h"
#include "nce.h"
#include "gpu.h"
#include <tinyxml2.h>

namespace skyline {
    void Mutex::lock() {
        while (flag.exchange(true, std::memory_order_relaxed));
        std::atomic_thread_fence(std::memory_order_acquire);
    }

    void Mutex::unlock() {
        std::atomic_thread_fence(std::memory_order_release);
        flag.store(false, std::memory_order_relaxed);
    }

    bool Mutex::try_lock() {
        bool fal = false;
        return flag.compare_exchange_strong(fal, true, std::memory_order_relaxed);
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

    DeviceState::DeviceState(kernel::OS *os, std::shared_ptr<kernel::type::KProcess> &thisProcess, std::shared_ptr<kernel::type::KThread> &thisThread, std::shared_ptr<JvmManager> jvmManager, std::shared_ptr<Settings> settings, std::shared_ptr<Logger> logger)
        : os(os), jvmManager(std::move(jvmManager)), settings(std::move(settings)), logger(std::move(logger)), process(thisProcess), thread(thisThread) {
        // We assign these later as they use the state in their constructor and we don't want null pointers
        nce = std::move(std::make_shared<NCE>(*this));
        gpu = std::move(std::make_shared<gpu::GPU>(*this));
    }
}
