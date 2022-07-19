// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "settings.h"

namespace skyline {
    /**
     * @note This is a placeholder implementation, it must be overridden via template specialisation for platform-specific behavior
     */
    template<class T>
    void Settings::Update(T newSettings) {}
}
