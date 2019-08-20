#pragma once

#include <map>
#include <fstream>
#include <syslog.h>
#include <string>
#include <sstream>
#include <memory>
#include <fmt/format.h>
#include "hw/cpu.h"
#include "hw/memory.h"
#include "constant.h"

namespace lightSwitch {
    class Settings {
    private:
        struct KeyCompare {
            bool operator()(char const *a, char const *b) const {
                return std::strcmp(a, b) < 0;
            }
        };

        std::map<char *, char *, KeyCompare> string_map;
        std::map<char *, bool, KeyCompare> bool_map;

    public:
        Settings(std::string pref_xml);

        char *GetString(char *key);

        bool GetBool(char *key);

        void List();
    };

    class Logger {
    private:
        std::ofstream log_file;
        const char *level_str[4] = {"0", "1", "2", "3"};
        int level_syslog[4] = {LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG};

    public:
        enum LogLevel {
            ERROR,
            WARN,
            INFO,
            DEBUG
        };

        Logger(std::string log_path);

        ~Logger();

        void write_header(std::string str);

        void write(LogLevel level, std::string str);

        template<typename S, typename... Args>
        void write(Logger::LogLevel level, const S &format_str, Args &&... args) {
            write(level, fmt::format(format_str, args...));
        }
    };

    struct device_state {
        std::shared_ptr<hw::Cpu> cpu;
        std::shared_ptr<hw::Memory> memory;
        std::shared_ptr<Settings> settings;
        std::shared_ptr<Logger> logger;
    };
}