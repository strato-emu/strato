// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/log.h>
#include <tinyxml2.h>
#include "settings.h"

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
            logger->Info("{} = \"{}\"", iter.first, GetString(iter.first));

        for (auto &iter : boolMap)
            logger->Info("{} = {}", iter.first, GetBool(iter.first));
    }
}
