// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)


#include <soc.h>
#include "nvhost_syncpoint.h"

namespace skyline::service::nvdrv {
    NvHostSyncpoint::NvHostSyncpoint(const DeviceState &state) : state(state) {
        constexpr u32 VBlank0SyncpointId{26};
        constexpr u32 VBlank1SyncpointId{27};

        // Reserve both vblank syncpoints as client managed as they use Continuous Mode
        // Refer to section 14.3.5.3 of the TRM for more information on Continuous Mode
        // https://github.com/Jetson-TX1-AndroidTV/android_kernel_jetson_tx1_hdmi_primary/blob/8f74a72394efb871cb3f886a3de2998cd7ff2990/drivers/gpu/host1x/drm/dc.c#L660
        ReserveSyncpoint(VBlank0SyncpointId, true);
        ReserveSyncpoint(VBlank1SyncpointId, true);
    }

    u32 NvHostSyncpoint::ReserveSyncpoint(u32 id, bool clientManaged) {
        if (syncpoints.at(id).reserved)
            throw exception("Requested syncpoint is in use");

        syncpoints.at(id).reserved = true;
        syncpoints.at(id).interfaceManaged = clientManaged;

        return id;
    }

    u32 NvHostSyncpoint::FindFreeSyncpoint() {
        for (u32 i{1}; i < syncpoints.size(); i++)
            if (!syncpoints[i].reserved)
                return i;

        throw exception("Failed to find a free syncpoint!");
    }

    u32 NvHostSyncpoint::AllocateSyncpoint(bool clientManaged) {
        std::lock_guard lock(reservationLock);
        return ReserveSyncpoint(FindFreeSyncpoint(), clientManaged);
    }

    bool NvHostSyncpoint::HasSyncpointExpired(u32 id, u32 threshold) {
        const SyncpointInfo &syncpoint{syncpoints.at(id)};

        if (!syncpoint.reserved)
            throw exception("Cannot check the expiry status of an unreserved syncpoint!");

        // If the interface manages counters then we don't keep track of the maximum value as it handles sanity checking the values then
        if (syncpoint.interfaceManaged)
            return static_cast<i32>(syncpoint.counterMin - threshold) >= 0;
        else
            return (syncpoint.counterMax - threshold) >= (syncpoint.counterMin - threshold);
    }

    u32 NvHostSyncpoint::IncrementSyncpointMaxExt(u32 id, u32 amount) {
        if (!syncpoints.at(id).reserved)
            throw exception("Cannot increment an unreserved syncpoint!");

        return syncpoints.at(id).counterMax += amount;
    }

    u32 NvHostSyncpoint::ReadSyncpointMinValue(u32 id) {
        if (!syncpoints.at(id).reserved)
            throw exception("Cannot read an unreserved syncpoint!");

        return syncpoints.at(id).counterMin;
    }

    u32 NvHostSyncpoint::UpdateMin(u32 id) {
        if (!syncpoints.at(id).reserved)
            throw exception("Cannot update an unreserved syncpoint!");

        syncpoints.at(id).counterMin = state.soc->host1x.syncpoints.at(id).Load();
        return syncpoints.at(id).counterMin;
    }
}
