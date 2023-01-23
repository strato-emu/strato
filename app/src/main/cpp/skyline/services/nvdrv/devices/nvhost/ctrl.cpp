// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2019-2020 Ryujinx Team and Contributors (https://github.com/Ryujinx/)

#include <soc.h>
#include <services/nvdrv/devices/deserialisation/deserialisation.h>
#include "ctrl.h"

namespace skyline::service::nvdrv::device::nvhost {
    Ctrl::SyncpointEvent::SyncpointEvent(const DeviceState &state) : event(std::make_shared<type::KEvent>(state, false)) {}

    void Ctrl::SyncpointEvent::Signal() {
        // We should only signal the KEvent if the event is actively being waited on
        if (state.exchange(State::Signalled) == State::Waiting)
            event->Signal();
    }

    void Ctrl::SyncpointEvent::Cancel(soc::host1x::Host1x &host1x) {
        host1x.syncpoints.at(fence.id).host.DeregisterWaiter(waiterHandle);
        waiterHandle = {};
    }

    void Ctrl::SyncpointEvent::RegisterWaiter(soc::host1x::Host1x &host1x, const Fence &pFence) {
        fence = pFence;
        state = State::Waiting;
        waiterHandle = host1x.syncpoints.at(fence.id).host.RegisterWaiter(fence.threshold, [this] { Signal(); });
    }

    bool Ctrl::SyncpointEvent::IsInUse() {
        return state == SyncpointEvent::State::Waiting ||
            state == SyncpointEvent::State::Cancelling ||
            state == SyncpointEvent::State::Signalling;
    }

    Ctrl::Ctrl(const DeviceState &state, Driver &driver, Core &core, const SessionContext &ctx) : NvDevice(state, driver, core, ctx) {}

    u32 Ctrl::FindFreeSyncpointEvent(u32 syncpointId) {
        u32 eventSlot{SyncpointEventCount}; //!< Holds the slot of the last populated event in the event array
        u32 freeSlot{SyncpointEventCount}; //!< Holds the slot of the first unused event id

        for (u32 i{}; i < SyncpointEventCount; i++) {
            if (syncpointEvents[i]) {
                const auto &event{syncpointEvents[i]};

                if (!event->IsInUse()) {
                    eventSlot = i;

                    // This event is already attached to the requested syncpoint, so use it
                    if (event->fence.id == syncpointId)
                        return eventSlot;
                }
            } else if (freeSlot == SyncpointEventCount) {
                freeSlot = i;
            }
        }

        // Use an unused event if possible
        if (freeSlot < SyncpointEventCount) {
            syncpointEvents[freeSlot] = std::make_unique<SyncpointEvent>(state);
            return freeSlot;
        }

        // Recycle an existing event if all else fails
        if (eventSlot < SyncpointEventCount)
            return eventSlot;

        throw exception("Failed to find a free nvhost event!");
    }

    PosixResult Ctrl::SyncpointWaitEventImpl(In<Fence> fence, In<i32> timeout, InOut<SyncpointEventValue> value, bool allocate) {
        Logger::Debug("fence: ( id: {}, threshold: {} ), timeout: {}, value: {}, allocate: {}",
                            fence.id, fence.threshold, timeout, value.val, allocate);

        if (fence.id >= soc::host1x::SyncpointCount)
            return PosixResult::InvalidArgument;

        // No need to wait since syncpoints start at 0
        if (fence.threshold == 0) {
            // oss-nvjpg waits on syncpoint 0 during initialisation without reserving it, this is technically valid with a zero threshold but could also be a sign of a bug on our side in other cases, hence the warn
            if (!core.syncpointManager.IsSyncpointAllocated(fence.id))
                Logger::Warn("Tried to wait on an unreserved syncpoint with no threshold");

            return PosixResult::Success;
        }

        // Check if the syncpoint has already expired using the last known values
        if (core.syncpointManager.IsFenceSignalled(fence)) {
            value.val = core.syncpointManager.ReadSyncpointMinValue(fence.id);
            return PosixResult::Success;
        }

        // Sync the syncpoint with the GPU then check again
        auto minVal{core.syncpointManager.UpdateMin(fence.id)};
        if (core.syncpointManager.IsFenceSignalled(fence)) {
            value.val = minVal;
            return PosixResult::Success;
        }

        // Don't try to register any waits if there is no timeout for them
        if (!timeout)
            return PosixResult::TryAgain;

        std::scoped_lock lock{syncpointEventMutex};

        u32 slot = [&]() {
            if (allocate) {
                value.val = 0;
                return FindFreeSyncpointEvent(fence.id);
            } else {
                return value.val;
            }
        }();

        if (slot >= SyncpointEventCount)
            return PosixResult::InvalidArgument;

        auto &event{syncpointEvents[slot]};
        if (!event)
            return PosixResult::InvalidArgument;

        if (!event->IsInUse()) {
            Logger::Debug("Waiting on syncpoint event: {} with fence: ({}, {})", slot, fence.id, fence.threshold);
            event->RegisterWaiter(state.soc->host1x, fence);

            value.val = 0;

            if (allocate) {
                value.syncpointIdForAllocation = static_cast<u16>(fence.id);
                value.eventAllocated = true;
            } else {
                value.syncpointId = fence.id;
            }

            // Slot will overwrite some of syncpointId here... it makes no sense for Nvidia to do this
            value.val |= slot;

            return PosixResult::TryAgain;
        } else {
            return PosixResult::InvalidArgument;
        }
    }

    PosixResult Ctrl::SyncpointFreeEventLocked(In<u32> slot) {
        if (slot >= SyncpointEventCount)
            return PosixResult::InvalidArgument;

        auto &event{syncpointEvents[slot]};
        if (!event)
            return PosixResult::Success; // If the event doesn't already exist then we don't need to do anything

        if (event->IsInUse()) // Avoid freeing events when they are still waiting etc.
            return PosixResult::Busy;

        syncpointEvents[slot] = nullptr;

        return PosixResult::Success;
    }

    PosixResult Ctrl::SyncpointClearEventWait(In<SyncpointEventValue> value) {
        Logger::Debug("slot: {}", value.slot);

        u16 slot{value.slot};
        if (slot >= SyncpointEventCount)
            return PosixResult::InvalidArgument;

        std::scoped_lock lock{syncpointEventMutex};

        auto &event{syncpointEvents[slot]};
        if (!event)
            return PosixResult::InvalidArgument;

        if (event->state.exchange(SyncpointEvent::State::Cancelling) == SyncpointEvent::State::Waiting) {
            Logger::Debug("Cancelling waiting syncpoint event: {}", slot);
            event->Cancel(state.soc->host1x);
            core.syncpointManager.UpdateMin(event->fence.id);
        }

        event->state = SyncpointEvent::State::Cancelled;
        event->event->ResetSignal();

        return PosixResult::Success;
    }

    PosixResult Ctrl::SyncpointWaitEvent(In<Fence> fence, In<i32> timeout, InOut<SyncpointEventValue> value) {
        return SyncpointWaitEventImpl(fence, timeout, value, true);
    }

    PosixResult Ctrl::SyncpointWaitEventSingle(In<Fence> fence, In<i32> timeout, InOut<SyncpointEventValue> value) {
        return SyncpointWaitEventImpl(fence, timeout, value, false);
    }

    PosixResult Ctrl::SyncpointAllocateEvent(In<u32> slot) {
        Logger::Debug("slot: {}", slot);

        if (slot >= SyncpointEventCount)
            return PosixResult::InvalidArgument;

        std::scoped_lock lock{syncpointEventMutex};

        auto &event{syncpointEvents[slot]};
        if (event) // Recreate event if it already exists
            if (auto err{SyncpointFreeEventLocked(slot)}; err != PosixResult::Success)
                return err;

        event = std::make_unique<SyncpointEvent>(state);

        return PosixResult::Success;
    }

    PosixResult Ctrl::SyncpointFreeEvent(In<u32> slot) {
        Logger::Debug("slot: {}", slot);

        std::scoped_lock lock{syncpointEventMutex};
        return SyncpointFreeEventLocked(slot);
    }

    PosixResult Ctrl::SyncpointFreeEventBatch(In<u64> bitmask) {
        Logger::Debug("bitmask: 0x{:X}", bitmask);

        auto err{PosixResult::Success};

        // Avoid repeated locks/unlocks by just locking now
        std::scoped_lock lock{syncpointEventMutex};

        for (u32 i{}; i < std::numeric_limits<u64>::digits; i++)
            if (bitmask & (1ULL << i))
                if (auto freeErr{SyncpointFreeEventLocked(i)}; freeErr != PosixResult::Success)
                    err = freeErr;

        return err;
    }

    std::shared_ptr<type::KEvent> Ctrl::QueryEvent(u32 valueRaw) {
        SyncpointEventValue value{.val = valueRaw};

        // I have no idea why nvidia does this
        u16 slot{value.eventAllocated ? static_cast<u16>(value.partialSlot) : value.slot};
        if (slot >= SyncpointEventCount)
            return nullptr;

        u32 syncpointId{value.eventAllocated ? static_cast<u32>(value.syncpointIdForAllocation) : value.syncpointId};

        std::scoped_lock lock{syncpointEventMutex};

        auto &event{syncpointEvents[slot]};
        if (event && event->fence.id == syncpointId)
            return event->event;

        return nullptr;
    }

#include <services/nvdrv/devices/deserialisation/macro_def.inc>
    static constexpr u32 CtrlMagic{0};

    IOCTL_HANDLER_FUNC(Ctrl, ({
        IOCTL_CASE_ARGS(INOUT, SIZE(0x4),  MAGIC(CtrlMagic), FUNC(0x1C),
                        SyncpointClearEventWait,  ARGS(In<SyncpointEventValue>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x10), MAGIC(CtrlMagic), FUNC(0x1D),
                        SyncpointWaitEvent,       ARGS(In<Fence>, In<i32>, InOut<SyncpointEventValue>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x10), MAGIC(CtrlMagic), FUNC(0x1E),
                        SyncpointWaitEventSingle, ARGS(In<Fence>, In<i32>, InOut<SyncpointEventValue>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x4),  MAGIC(CtrlMagic), FUNC(0x1F),
                        SyncpointAllocateEvent,   ARGS(In<u32>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x4),  MAGIC(CtrlMagic), FUNC(0x20),
                        SyncpointFreeEvent,       ARGS(In<u32>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x8),  MAGIC(CtrlMagic), FUNC(0x21),
                        SyncpointFreeEventBatch,  ARGS(In<u64>))

        IOCTL_CASE_RESULT(INOUT, SIZE(0x183), MAGIC(CtrlMagic), FUNC(0x1B),
                          PosixResult::InvalidArgument) // GetConfig isn't available in production
    }))
#include <services/nvdrv/devices/deserialisation/macro_undef.inc>
}
