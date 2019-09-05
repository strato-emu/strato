#include "common.h"
#include <tinyxml2.h>
#include <syslog.h>

namespace lightSwitch {
    namespace memory {
        Permission::Permission() {
            r = 0;
            w = 0;
            x = 0;
        }

        Permission::Permission(bool read, bool write, bool execute) {
            r = read;
            w = write;
            x = execute;
        }

        bool Permission::operator==(const Permission &rhs) const { return (this->r == rhs.r && this->w == rhs.w && this->x == rhs.x); }

        bool Permission::operator!=(const Permission &rhs) const { return !operator==(rhs); }

        int Permission::get() const {
            int perm = 0;
            if (r) perm |= PROT_READ;
            if (w) perm |= PROT_WRITE;
            if (x) perm |= PROT_EXEC;
            return perm;
        }
    }

    Settings::Settings(std::string pref_xml) {
        tinyxml2::XMLDocument pref;
        if (pref.LoadFile(pref_xml.c_str()))
            throw exception("TinyXML2 Error: " + std::string(pref.ErrorStr()));
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

    Logger::Logger(const std::string & log_path) {
        log_file.open(log_path, std::ios::app);
        WriteHeader("Logging started");
    }

    Logger::~Logger() {
        WriteHeader("Logging ended");
    }

    void Logger::WriteHeader(const std::string& str) {
        syslog(LOG_ALERT, "%s", str.c_str());
        log_file << "0|" << str << "\n";
        log_file.flush();
    }

    void Logger::Write(const LogLevel level, const std::string& str)  {
        #ifdef NDEBUG
        if (level == DEBUG) return;
        #endif
        syslog(level_syslog[level], "%s", str.c_str());
        log_file << "1|" << level_str[level] << "|" << str << "\n";
        log_file.flush();
    }
}
