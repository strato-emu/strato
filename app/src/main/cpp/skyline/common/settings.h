// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline {
    /**
     * @brief The Settings class is used to access preferences set in the Kotlin component of Skyline
     */
    class Settings {
      private:
        std::unordered_map<std::string, std::string> stringMap; //!< A mapping from all keys to their corresponding string value
        std::unordered_map<std::string, bool> boolMap; //!< A mapping from all keys to their corresponding boolean value
        std::unordered_map<std::string, int> intMap; //!< A mapping from all keys to their corresponding integer value

      public:
        /**
         * @param fd An FD to the preference XML file
         */
        Settings(int fd);

        /**
         * @brief Retrieves a particular setting as a string
         * @param key The key of the setting
         * @return The string value of the setting
         */
        std::string GetString(const std::string &key);

        /**
         * @brief Retrieves a particular setting as a boolean
         * @param key The key of the setting
         * @return The boolean value of the setting
         */
        bool GetBool(const std::string &key);

        /**
         * @brief Retrieves a particular setting as a integer
         * @param key The key of the setting
         * @return The integer value of the setting
         */
        int GetInt(const std::string &key);

        /**
         * @brief Writes all settings keys and values to syslog, this function is for development purposes
         */
        void List(const std::shared_ptr<Logger> &logger);
    };
}
