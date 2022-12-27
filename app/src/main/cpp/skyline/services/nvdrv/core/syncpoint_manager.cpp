// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2019-2020 Ryujinx Team and Contributors (https://github.com/Ryujinx/)

#include <soc.h>
#include "syncpoint_manager.h"

namespace skyline::service::nvdrv::core {
    SyncpointManager::SyncpointManager(const DeviceState &state) : state(state) {
        constexpr u32 VBlank0SyncpointId{26};
        constexpr u32 VBlank1SyncpointId{27};

        // Reserve both vblank syncpoints as client managed as they use Continuous Mode
        // Refer to section 14.3.5.3 of the TRM for more information on Continuous Mode
        // https://github.com/Jetson-TX1-AndroidTV/android_kernel_jetson_tx1_hdmi_primary/blob/8f74a72394efb871cb3f886a3de2998cd7ff2990/drivers/gpu/host1x/drm/dc.c#L660
        ReserveSyncpoint(VBlank0SyncpointId, true);
        ReserveSyncpoint(VBlank1SyncpointId, true);

        for (u32 syncpointId : ChannelSyncpoints)
            if (syncpointId)
                ReserveSyncpoint(syncpointId, false);
    }

    u32 SyncpointManager::ReserveSyncpoint(u32 id, bool clientManaged) {
        if (syncpoints.at(id).reserved)
            throw exception("Requested syncpoint is in use");

        syncpoints.at(id).reserved = true;
        syncpoints.at(id).interfaceManaged = clientManaged;

        return id;
    }

    u32 SyncpointManager::FindFreeSyncpoint() {
        for (u32 i{1}; i < syncpoints.size(); i++)
            if (!syncpoints[i].reserved)
                return i;

        throw exception("Failed to find a free syncpoint!");
    }

    u32 SyncpointManager::AllocateSyncpoint(bool clientManaged) {
        std::scoped_lock lock{reservationLock};
        return ReserveSyncpoint(FindFreeSyncpoint(), clientManaged);
    }

    bool SyncpointManager::IsSyncpointAllocated(u32 id) {
        return (id <= soc::host1x::SyncpointCount) && syncpoints[id].reserved;
    }

    bool SyncpointManager::HasSyncpointExpired(u32 id, u32 threshold) {
        const SyncpointInfo &syncpoint{syncpoints.at(id)};

        if (!syncpoint.reserved)
            throw exception("Cannot check the expiry status of an unreserved syncpoint!");

        // If the interface manages counters then we don't keep track of the maximum value as it handles sanity checking the values then
        if (syncpoint.interfaceManaged)
            return static_cast<i32>(syncpoint.counterMin - threshold) >= 0;
        else
            return (syncpoint.counterMax - threshold) >= (syncpoint.counterMin - threshold);
    }

    u32 SyncpointManager::IncrementSyncpointMaxExt(u32 id, u32 amount) {
        if (!syncpoints.at(id).reserved)
            throw exception("Cannot increment an unreserved syncpoint!");

        return syncpoints.at(id).counterMax += amount;
    }

    u32 SyncpointManager::ReadSyncpointMinValue(u32 id) {
        if (!syncpoints.at(id).reserved)
            throw exception("Cannot read an unreserved syncpoint!");

        return syncpoints.at(id).counterMin;
    }

    u32 SyncpointManager::UpdateMin(u32 id) {
        if (!syncpoints.at(id).reserved)
            throw exception("Cannot update an unreserved syncpoint!");

        syncpoints.at(id).counterMin = state.soc->host1x.syncpoints.at(id).host.Load();
        return syncpoints.at(id).counterMin;
    }

    Fence SyncpointManager::GetSyncpointFence(u32 id) {
        if (!syncpoints.at(id).reserved)
            throw exception("Cannot access an unreserved syncpoint!");

        return {
            .id = id,
            .threshold = syncpoints.at(id).counterMax
        };
    }
}
