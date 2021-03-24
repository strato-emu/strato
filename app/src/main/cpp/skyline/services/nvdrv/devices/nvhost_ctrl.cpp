// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2019-2020 Ryujinx Team and Contributors

#include <soc.h>
#include <kernel/types/KProcess.h>
#include <services/nvdrv/driver.h>
#include "nvhost_ctrl.h"

namespace skyline::service::nvdrv::device {
    SyncpointEvent::SyncpointEvent(const DeviceState &state) : event(std::make_shared<type::KEvent>(state, false)) {}

    /**
     * @brief Metadata about a syncpoint event, used by QueryEvent and SyncpointEventWait
     */
    union SyncpointEventValue {
        u32 val;

        struct {
            u8 _pad0_ : 4;
            u32 syncpointIdAsync : 28;
        };

        struct {
            union {
                u8 eventSlotAsync;
                u16 eventSlotNonAsync;
            };
            u16 syncpointIdNonAsync : 12;
            bool nonAsync : 1;
            u8 _pad12_ : 3;
        };
    };
    static_assert(sizeof(SyncpointEventValue) == sizeof(u32));

    void SyncpointEvent::Signal() {
        std::lock_guard lock(mutex);

        auto oldState{state};
        state = State::Signalling;

        // We should only signal the KEvent if the event is actively being waited on
        if (oldState == State::Waiting)
            event->Signal();

        state = State::Signalled;
    }

    void SyncpointEvent::Cancel(soc::host1x::Host1X &host1x) {
        std::lock_guard lock(mutex);

        host1x.syncpoints.at(fence.id).DeregisterWaiter(waiterId);
        Signal();
        event->ResetSignal();
    }

    void SyncpointEvent::Wait(soc::host1x::Host1X &host1x, const Fence &pFence) {
        std::lock_guard lock(mutex);

        fence = pFence;
        state = State::Waiting;
        waiterId = host1x.syncpoints.at(fence.id).RegisterWaiter(fence.value, [this] { Signal(); });
    }

    NvHostCtrl::NvHostCtrl(const DeviceState &state) : NvDevice(state) {}

    u32 NvHostCtrl::FindFreeSyncpointEvent(u32 syncpointId) {
        u32 eventSlot{constant::NvHostEventCount}; //!< Holds the slot of the last populated event in the event array
        u32 freeSlot{constant::NvHostEventCount}; //!< Holds the slot of the first unused event id
        std::lock_guard lock(syncpointEventMutex);

        for (u32 i{}; i < constant::NvHostEventCount; i++) {
            if (syncpointEvents[i]) {
                auto event{syncpointEvents[i]};

                if (event->state == SyncpointEvent::State::Cancelled || event->state == SyncpointEvent::State::Available || event->state == SyncpointEvent::State::Signalled) {
                    eventSlot = i;

                    // This event is already attached to the requested syncpoint, so use it
                    if (event->fence.id == syncpointId)
                        return eventSlot;
                }
            } else if (freeSlot == constant::NvHostEventCount) {
                freeSlot = i;
            }
        }

        // Use an unused event if possible
        if (freeSlot < constant::NvHostEventCount) {
            syncpointEvents[eventSlot] = std::make_shared<SyncpointEvent>(state);
            return freeSlot;
        }

        // Recycle an existing event if all else fails
        if (eventSlot < constant::NvHostEventCount)
            return eventSlot;

        throw exception("Failed to find a free nvhost event!");
    }

    NvStatus NvHostCtrl::SyncpointEventWaitImpl(span<u8> buffer, bool async) {
        struct Data {
            Fence fence;               // In
            u32 timeout;               // In
            SyncpointEventValue value; // InOut
        } &data = buffer.as<Data>();

        if (data.fence.id >= soc::host1x::SyncpointCount)
            return NvStatus::BadValue;

        if (data.timeout == 0)
            return NvStatus::Timeout;

        auto driver{nvdrv::driver.lock()};
        auto &hostSyncpoint{driver->hostSyncpoint};

        // Check if the syncpoint has already expired using the last known values
        if (hostSyncpoint.HasSyncpointExpired(data.fence.id, data.fence.value)) {
            data.value.val = hostSyncpoint.ReadSyncpointMinValue(data.fence.id);
            return NvStatus::Success;
        }

        // Sync the syncpoint with the GPU then check again
        auto minVal{hostSyncpoint.UpdateMin(data.fence.id)};
        if (hostSyncpoint.HasSyncpointExpired(data.fence.id, data.fence.value)) {
            data.value.val = minVal;
            return NvStatus::Success;
        }

        u32 eventSlot{};
        if (async) {
            if (data.value.val >= constant::NvHostEventCount)
                return NvStatus::BadValue;

            eventSlot = data.value.val;
        } else {
            data.fence.value = 0;

            eventSlot = FindFreeSyncpointEvent(data.fence.id);
        }

        std::lock_guard lock(syncpointEventMutex);

        auto event{syncpointEvents[eventSlot]};
        if (!event)
            return NvStatus::BadValue;

        std::lock_guard eventLock(event->mutex);

        if (event->state == SyncpointEvent::State::Cancelled || event->state == SyncpointEvent::State::Available || event->state == SyncpointEvent::State::Signalled) {
            state.logger->Debug("Waiting on syncpoint event: {} with fence: ({}, {})", eventSlot, data.fence.id, data.fence.value);
            event->Wait(state.soc->host1x, data.fence);

            data.value.val = 0;

            if (async) {
                data.value.syncpointIdAsync = data.fence.id;
            } else {
                data.value.syncpointIdNonAsync = data.fence.id;
                data.value.nonAsync = true;
            }

            data.value.val |= eventSlot;

            return NvStatus::Timeout;
        } else {
            return NvStatus::BadValue;
        }
    }

    NvStatus NvHostCtrl::GetConfig(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return NvStatus::BadValue;
    }

    NvStatus NvHostCtrl::SyncpointClearEventWait(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        auto eventSlot{buffer.as<u16>()};

        if (eventSlot >= constant::NvHostEventCount)
            return NvStatus::BadValue;

        std::lock_guard lock(syncpointEventMutex);

        auto event{syncpointEvents[eventSlot]};
        if (!event)
            return NvStatus::BadValue;

        std::lock_guard eventLock(event->mutex);

        if (event->state == SyncpointEvent::State::Waiting) {
            event->state = SyncpointEvent::State::Cancelling;
            state.logger->Debug("Cancelling waiting syncpoint event: {}", eventSlot);
            event->Cancel(state.soc->host1x);
        }

        event->state = SyncpointEvent::State::Cancelled;

        auto driver{nvdrv::driver.lock()};
        auto &hostSyncpoint{driver->hostSyncpoint};
        hostSyncpoint.UpdateMin(event->fence.id);

        return NvStatus::Success;
    }

    NvStatus NvHostCtrl::SyncpointEventWait(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return SyncpointEventWaitImpl(buffer, false);
    }

    NvStatus NvHostCtrl::SyncpointEventWaitAsync(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return SyncpointEventWaitImpl(buffer, true);
    }

    NvStatus NvHostCtrl::SyncpointRegisterEvent(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        auto eventSlot{buffer.as<u32>()};
        state.logger->Debug("Registering syncpoint event: {}", eventSlot);

        if (eventSlot >= constant::NvHostEventCount)
            return NvStatus::BadValue;

        std::lock_guard lock(syncpointEventMutex);

        auto &event{syncpointEvents[eventSlot]};
        if (event)
            throw exception("Recreating events is unimplemented");

        event = std::make_shared<SyncpointEvent>(state);

        return NvStatus::Success;
    }

    std::shared_ptr<type::KEvent> NvHostCtrl::QueryEvent(u32 eventId) {
        SyncpointEventValue eventValue{.val = eventId};
        std::lock_guard lock(syncpointEventMutex);

        auto event{syncpointEvents.at(eventValue.nonAsync ? eventValue.eventSlotNonAsync : eventValue.eventSlotAsync)};

        if (event && event->fence.id == (eventValue.nonAsync ? eventValue.syncpointIdNonAsync : eventValue.syncpointIdAsync))
            return event->event;

        return nullptr;
    }
}
