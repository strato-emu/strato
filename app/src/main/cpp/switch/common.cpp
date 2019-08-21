#include "common.h"
#include <tinyxml2.h>
#include <syslog.h>

namespace lightSwitch {
    // Settings
    Settings::Settings(std::string pref_xml) {
        tinyxml2::XMLDocument pref;
        if (pref.LoadFile(pref_xml.c_str())) {
            syslog(LOG_ERR, "TinyXML2 Error: %s", pref.ErrorStr());
            throw pref.ErrorID();
        }
        tinyxml2::XMLElement *elem = pref.LastChild()->FirstChild()->ToElement();
        while (elem) {
            switch (elem->Value()[0]) {
                case 's':
                    string_map.insert(
                            std::pair<char *, char *>((char *) elem->FindAttribute("name")->Value(),
                                                      (char *) elem->GetText()));
                    break;
                case 'b':
                    bool_map.insert(
                            std::pair<char *, bool>((char *) elem->FindAttribute("name")->Value(),
                                                    elem->FindAttribute("value")->BoolValue()));
                default:
                    break;
            };
            if (elem->NextSibling())
                elem = elem->NextSibling()->ToElement();
            else break;
        }
        pref.Clear();
    }

    char *Settings::GetString(char *key) {
        return string_map.at(key);
    }

    bool Settings::GetBool(char *key) {
        return bool_map.at(key);
    }

    void Settings::List() {
        auto it_s = string_map.begin();
        while (it_s != string_map.end()) {
            syslog(LOG_INFO, "Key: %s", it_s->first);
            syslog(LOG_INFO, "Value: %s", GetString(it_s->first));
            it_s++;
        }
        auto it_b = bool_map.begin();
        while (it_b != bool_map.end()) {
            syslog(LOG_INFO, "Key: %s", it_b->first);
            syslog(LOG_INFO, "Value: %i", GetBool(it_b->first));
            it_b++;
        }
    }

    // Logger
    Logger::Logger(std::string log_path) {
        log_file.open(log_path, std::ios::app);
        write_header("Logging started");
    }

    Logger::~Logger() {
        write_header("Logging ended");
    }

    void Logger::write(Logger::LogLevel level, std::string str) {
        if (level == DEBUG && debug_build)
            return;
        syslog(level_syslog[level], "%s", str.c_str());
        log_file << "1|" << level_str[level] << "|" << str << "\n";
        log_file.flush();
    }

    void Logger::write_header(std::string str) {
        log_file << "0|" << str << "\n";
        log_file.flush();
    }
}