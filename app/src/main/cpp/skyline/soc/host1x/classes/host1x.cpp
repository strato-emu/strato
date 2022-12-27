// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common.h>
#include "class.h"
#include "host1x.h"

namespace skyline::soc::host1x {
    Host1xClass::Host1xClass(SyncpointSet &syncpoints) : syncpoints(syncpoints) {}

    void Host1xClass::CallMethod(u32 method, u32 argument) {
        constexpr static u32 LoadSyncpointPayload32MethodId{0x4E}; //!< See '14.3.2.12 32-Bit Sync Point Comparison Methods' in TRM
        constexpr static u32 WaitSyncpoint32MethodId{0x50}; //!< As above

        switch (method) {
            case IncrementSyncpointMethodId: {
                IncrementSyncpointMethod incrSyncpoint{.raw = argument};

                // incrSyncpoint.condition doesn't matter for Host1x class increments
                Logger::Debug("Increment syncpoint: {}", incrSyncpoint.index);
                syncpoints.at(incrSyncpoint.index).Increment();
                break;
            }

            case LoadSyncpointPayload32MethodId:
                syncpointPayload = argument;
                break;

            case WaitSyncpoint32MethodId: {
                u32 syncpointId{static_cast<u8>(argument)};
                Logger::Debug("Wait syncpoint: {}, thresh: {}", syncpointId, syncpointPayload);

                syncpoints.at(syncpointId).host.Wait(syncpointPayload, std::chrono::steady_clock::duration::max());
                break;
            }

            default:
                Logger::Error("Unknown host1x class method called: 0x{:X}", method);
                break;
        }
    }
}
