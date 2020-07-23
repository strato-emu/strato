// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nvhost_syncpoint.h"

namespace skyline::service::nvdrv {
    NvHostSyncpoint::NvHostSyncpoint(const DeviceState &state) : state(state) {
        constexpr u32 VBlank0SyncpointId{26};
        constexpr u32 VBlank1SyncpointId{27};

        // Reserve both vblank syncpoints as client managed since the userspace driver has direct access to them
        ReserveSyncpoint(VBlank0SyncpointId, true);
        ReserveSyncpoint(VBlank1SyncpointId, true);
    }

    u32 NvHostSyncpoint::ReserveSyncpoint(u32 id, bool clientManaged) {
        if (id >= constant::MaxHwSyncpointCount)
            throw exception("Requested syncpoint ID is too high");

        if (syncpoints.at(id).assigned)
            throw exception("Requested syncpoint is in use");

        syncpoints.at(id).assigned = true;
        syncpoints.at(id).clientManaged = clientManaged;

        return id;
    }

    u32 NvHostSyncpoint::FindFreeSyncpoint() {
        for (u32 i = 0; i < constant::MaxHwSyncpointCount; i++)
            if (!syncpoints[i].assigned)
                return i;

        throw exception("Failed to find a free syncpoint!");
    }

    u32 NvHostSyncpoint::AllocateSyncpoint(bool clientManaged) {
        std::lock_guard lock(reservationLock);
        return ReserveSyncpoint(FindFreeSyncpoint(), clientManaged);
    }

    bool NvHostSyncpoint::HasSyncpointExpired(u32 id, u32 threshold) {
        const SyncpointInfo &syncpoint = syncpoints.at(id);

        if (syncpoint.clientManaged)
            return static_cast<i32>(syncpoint.counterMin - threshold) >= 0;
        else
            return (syncpoint.counterMax - threshold) >= (syncpoint.counterMin - threshold);
    }

    u32 NvHostSyncpoint::IncrementSyncpointMaxExt(u32 id, u32 amount) {
        return syncpoints.at(id).counterMax += amount;
    }
}
