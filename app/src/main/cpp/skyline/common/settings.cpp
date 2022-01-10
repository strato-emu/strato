// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "settings.h"

namespace skyline {
    void Settings::Subscribe(Callback callback) {
        callbacks.push_back(std::move(callback));
    }

    void Settings::OnSettingsChanged() {
        std::for_each(callbacks.begin(), callbacks.end(), [&](const Callback& listener) {
            listener(*this);
        });
    }

    /**
     * @note This is a placeholder implementation, it must be overridden via template specialisation for platform-specific behavior
     */
    template<class T>
    void Settings::Update(T newSettings) {}
}
