// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <input.h>
#include "touch.h"

namespace skyline::input {
    TouchManager::TouchManager(const DeviceState &state, input::HidSharedMemory *hid) : state(state), section(hid->touchScreen) {
        Activate();
    }

    void TouchManager::Activate() {
        activated = true;
        SetState({});
    }

    void TouchManager::SetState(const span<TouchScreenPoint> &points) {
        if (!activated)
            return;

        const auto& lastEntry = section.entries[section.header.currentEntry];
        auto entryIndex = (section.header.currentEntry != constant::HidEntryCount - 1) ? section.header.currentEntry + 1 : 0;
        auto& entry = section.entries[entryIndex];
        entry.globalTimestamp = lastEntry.globalTimestamp + 1;
        entry.localTimestamp = lastEntry.localTimestamp + 1;
        entry.touchCount = points.size();

        for (size_t i{}; i < points.size(); i++) {
            const auto& host = points[i];
            auto& guest = entry.data[i];
            guest.index = i;
            guest.positionX = host.x;
            guest.positionY = host.y;
            guest.minorAxis = host.minor;
            guest.majorAxis = host.major;
            guest.angle = host.angle;
        }

        section.header.timestamp = util::GetTimeTicks();
        section.header.entryCount = std::min(static_cast<u8>(section.header.entryCount + 1), constant::HidEntryCount);
        section.header.currentEntry = entryIndex;
    }
}
