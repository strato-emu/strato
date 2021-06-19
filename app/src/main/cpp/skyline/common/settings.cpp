// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#define PUGIXML_HEADER_ONLY

#include <pugixml.hpp>
#include "settings.h"

namespace skyline {
    Settings::Settings(int fd) {
        pugi::xml_document document;
        auto result{document.load_file(fmt::format("/proc/self/fd/{}", fd).c_str())};
        if (!result)
            throw exception("PugiXML Error: {} at {}", result.description(), result.offset);

        #define PREF_ELEM(name, memberName, rhs) std::make_pair(std::string(name), [](Settings &settings, const pugi::xml_node &element) { settings.memberName = rhs; })

        std::tuple preferences{
            PREF_ELEM("log_level", logLevel, static_cast<Logger::LogLevel>(element.text().as_uint(static_cast<unsigned int>(Logger::LogLevel::Info)))),
            PREF_ELEM("username_value", username, element.text().as_string()),
            PREF_ELEM("operation_mode", operationMode, element.attribute("value").as_bool()),
            PREF_ELEM("force_triple_buffering", forceTripleBuffering, element.attribute("value").as_bool()),
            PREF_ELEM("disable_frame_throttling", disableFrameThrottling, element.attribute("value").as_bool()),
        };

        #undef PREF_ELEM

        std::bitset<std::tuple_size_v<typeof(preferences)>> preferencesSet{}; // A bitfield to keep track of all the preferences we've set
        for (auto element{document.last_child().first_child()}; element; element = element.next_sibling()) {
            std::string_view name{element.attribute("name").value()};
            std::apply([&](auto... preferences) {
                size_t index{};
                ([&](auto preference) {
                    if (name.size() == preference.first.size() && name.starts_with(preference.first)) {
                        preference.second(*this, element);
                        preferencesSet.set(index);
                    }
                    index++;
                }(preferences), ...);
            }, preferences);
        }

        if (!preferencesSet.all()) {
            std::string unsetPreferences;
            std::apply([&](auto... preferences) {
                size_t index{};
                ([&](auto preference) {
                    if (!preferencesSet.test(index))
                        unsetPreferences += std::string("\n* ") + preference.first;
                    index++;
                }(preferences), ...);
            }, preferences);
            throw exception("Cannot find the following preferences:{}", unsetPreferences);
        }
    }
}
