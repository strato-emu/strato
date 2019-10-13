#include "common.h"
#include <tinyxml2.h>

namespace skyline {
    Settings::Settings(const std::string &prefXml) {
        tinyxml2::XMLDocument pref;
        if (pref.LoadFile(prefXml.c_str()))
            throw exception("TinyXML2 Error: " + std::string(pref.ErrorStr()));
        tinyxml2::XMLElement *elem = pref.LastChild()->FirstChild()->ToElement();
        while (elem) {
            switch (elem->Value()[0]) {
                case 's':
                    stringMap.insert(
                        std::pair<std::string, std::string>(elem->FindAttribute("name")->Value(), elem->GetText()));
                    break;
                case 'b':
                    boolMap.insert(std::pair<std::string, bool>(elem->FindAttribute("name")->Value(), elem->FindAttribute("value")->BoolValue()));
                default:
                    break;
            };
            if (elem->NextSibling())
                elem = elem->NextSibling()->ToElement();
            else break;
        }
        pref.Clear();
    }

    std::string Settings::GetString(const std::string& key) {
        return stringMap.at(key);
    }

    bool Settings::GetBool(const std::string& key) {
        return boolMap.at(key);
    }

    void Settings::List(std::shared_ptr<Logger> logger) {
        for (auto& iter : stringMap)
            logger->Write(Logger::Info, "Key: {}, Value: {}, Type: String", iter.first, GetString(iter.first));
        for (auto& iter : boolMap)
            logger->Write(Logger::Info, "Key: {}, Value: {}, Type: Bool", iter.first, GetBool(iter.first));
    }

    Logger::Logger(const std::string &logPath) {
        logFile.open(logPath, std::ios::app);
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

    void Logger::Write(const LogLevel level, const std::string& str) {
        #ifdef NDEBUG
        if (level == Debug) return;
        #endif
        syslog(levelSyslog[level], "%s", str.c_str());
        logFile << "1|" << levelStr[level] << "|" << str << "\n";
        logFile.flush();
    }
}
