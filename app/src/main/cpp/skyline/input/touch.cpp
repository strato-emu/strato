// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "touch.h"

namespace skyline::input {
    TouchManager::TouchManager(const DeviceState &state, input::HidSharedMemory *hid) : state(state), section(hid->touchScreen) {
        Activate(); // The touch screen is expected to be activated by default, commercial games are reliant on this behavior
    }

    void TouchManager::Activate() {
        if (!activated) {
            activated = true;
            SetState({});
        }
    }

    void TouchManager::SetState(span<TouchScreenPoint> touchPoints) {
        if (!activated)
            return;

        const auto &lastEntry{section.entries[section.header.currentEntry]};
        auto entryIndex{(section.header.currentEntry != constant::HidEntryCount - 1) ? section.header.currentEntry + 1 : 0};
        auto &entry{section.entries[entryIndex]};
        touchPoints = touchPoints.first(std::min(touchPoints.size(), entry.data.size()));
        entry.globalTimestamp = lastEntry.globalTimestamp + 1;
        entry.localTimestamp = lastEntry.localTimestamp + 1;
        entry.touchCount = touchPoints.size();

        for (size_t i{}; i < touchPoints.size(); i++) {
            const auto &host{touchPoints[i]};
            auto &guest{entry.data[i]};
            guest.attribute.raw = static_cast<u32>(host.attribute);
            guest.index = static_cast<u32>(host.id);
            guest.positionX = static_cast<u32>(host.x);
            guest.positionY = static_cast<u32>(host.y);
            guest.minorAxis = static_cast<u32>(host.minor);
            guest.majorAxis = static_cast<u32>(host.major);
            guest.angle = host.angle;
        }

        // Clear unused touch points
        for (size_t i{touchPoints.size()}; i < entry.data.size(); i++)
            entry.data[i] = {};

        section.header.timestamp = util::GetTimeTicks();
        section.header.entryCount = std::min(static_cast<u8>(section.header.entryCount + 1), constant::HidEntryCount);
        section.header.maxEntry = section.header.entryCount;
        section.header.currentEntry = entryIndex;
    }
}
