// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "touch.h"

namespace skyline::input {
    TouchManager::TouchManager(const DeviceState &state, input::HidSharedMemory *hid) : state(state), section(hid->touchScreen) {
        Activate(); // The touch screen is expected to be activated by default, commercial games are reliant on this behavior
    }

    void TouchManager::Activate() {
        std::scoped_lock lock{mutex};
        if (!activated) {
            activated = true;
            SetState({});
        }
    }

    void TouchManager::SetState(span<TouchScreenPoint> touchPoints) {
        std::scoped_lock lock{mutex};

        touchPoints = touchPoints.first(std::min(touchPoints.size(), screenState.data.size()));
        screenState.touchCount = touchPoints.size();

        for (size_t i{}; i < touchPoints.size(); i++) {
            const auto &host{touchPoints[i]};
            auto &guest{screenState.data[i]};

            constexpr uint8_t TouchPointTimeout{3}; //!< The amount of frames an ended point is expected to be active before it is removed from the screen

            guest.attribute.raw = static_cast<u32>(host.attribute);
            if (guest.attribute.end)
                pointTimeout[i] = TouchPointTimeout;

            guest.index = static_cast<u32>(host.id);
            guest.positionX = static_cast<u32>(host.x);
            guest.positionY = static_cast<u32>(host.y);
            guest.minorAxis = static_cast<u32>(host.minor);
            guest.majorAxis = static_cast<u32>(host.major);
            guest.angle = host.angle;
        }

        // Clear unused touch points
        for (size_t i{touchPoints.size()}; i < screenState.data.size(); i++)
            screenState.data[i] = {};
    }

    void TouchManager::UpdateSharedMemory() {
        std::scoped_lock lock{mutex};

        for (size_t i{}; i < screenState.data.size(); i++) {
            // Remove any touch points which have ended after they are timed out
            if (screenState.data[i].attribute.end) {
                auto &timeout{pointTimeout[i]};
                if (timeout > 0) {
                    // Tick the timeout counter
                    timeout--;
                } else {
                    // Erase the point from the screen
                    if (i != screenState.data.size() - 1) {
                        // Move every point after the one being removed to fill the gap
                        for (size_t j{i + 1}; j < screenState.data.size(); j++) {
                            screenState.data[j - 1] = screenState.data[j];
                            pointTimeout[j - 1] = pointTimeout[j];
                        }
                        i--;
                    }
                    screenState.touchCount--;
                }
            }
        }

        if (!activated)
            return;

        const auto &lastEntry{section.entries[section.header.currentEntry]};

        section.header.timestamp = util::GetTimeTicks();
        section.header.entryCount = std::min(static_cast<u8>(section.header.entryCount + 1), constant::HidEntryCount);
        section.header.maxEntry = section.header.entryCount - 1;
        section.header.currentEntry = (section.header.currentEntry < section.header.maxEntry) ? section.header.currentEntry + 1 : 0;

        auto &entry{section.entries[section.header.currentEntry]};
        entry = screenState;
        entry.globalTimestamp = lastEntry.globalTimestamp + 1;
        entry.localTimestamp = lastEntry.localTimestamp + 1;
    }
}
