// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <gpu.h>
#include <kernel/types/KProcess.h>
#include <services/nvdrv/driver.h>
#include "nvhost_ctrl.h"

namespace skyline::service::nvdrv::device {
    NvHostEvent::NvHostEvent(const DeviceState &state) : event(std::make_shared<type::KEvent>(state)) {}

    void NvHostEvent::Signal() {
        auto oldState = state;
        state = State::Signaling;

        // This is to ensure that the HOS event isn't signalled when the nvhost event is cancelled
        if (oldState == State::Waiting)
            event->Signal();

        state = State::Signaled;
    }

    void NvHostEvent::Cancel(const std::shared_ptr<gpu::GPU> &gpuState) {
        gpuState->syncpoints.at(fence.id).DeregisterWaiter(waiterId);
        Signal();
        event->ResetSignal();
    }

    void NvHostEvent::Wait(const std::shared_ptr<gpu::GPU> &gpuState, const Fence &fence) {
        waiterId = gpuState->syncpoints.at(fence.id).RegisterWaiter(fence.value, [this] { Signal(); });

        // If waiter ID is zero then the fence has already been hit and was signalled in the call to RegisterWaiter
        if (waiterId) {
            this->fence = fence;
            state = State::Waiting;
        }
    }

    NvHostCtrl::NvHostCtrl(const DeviceState &state) : NvDevice(state) {}

    u32 NvHostCtrl::FindFreeEvent(u32 syncpointId) {
        u32 eventIndex{constant::NvHostEventCount}; //!< Holds the index of the last populated event in the event array
        u32 freeIndex{constant::NvHostEventCount}; //!< Holds the index of the first unused event id

        for (u32 i{}; i < constant::NvHostEventCount; i++) {
            if (events[i]) {
                const auto &event = *events[i];

                if (event.state == NvHostEvent::State::Cancelled || event.state == NvHostEvent::State::Available || event.state == NvHostEvent::State::Signaled) {
                    eventIndex = i;

                    // This event is already attached to the requested syncpoint, so use it
                    if (event.fence.id == syncpointId)
                        return eventIndex;
                }
            } else if (freeIndex == constant::NvHostEventCount) {
                freeIndex = i;
            }
        }

        // Use an unused event if possible
        if (freeIndex < constant::NvHostEventCount) {
            events.at(freeIndex) = static_cast<const std::optional<NvHostEvent>>(NvHostEvent(state));
            return freeIndex;
        }

        // Recycle an existing event if all else fails
        if (eventIndex < constant::NvHostEventCount)
            return eventIndex;

        throw exception("Failed to find a free nvhost event!");
    }

    NvStatus NvHostCtrl::EventWaitImpl(span<u8> buffer, bool async) {
        struct Data {
            Fence fence;      // In
            u32 timeout;      // In
            EventValue value; // InOut
        } &data = buffer.as<Data>();

        if (data.fence.id >= constant::MaxHwSyncpointCount)
            return NvStatus::BadValue;

        if (data.timeout == 0)
            return NvStatus::Timeout;

        auto driver = nvdrv::driver.lock();
        auto &hostSyncpoint = driver->hostSyncpoint;

        // Check if the syncpoint has already expired using the last known values
        if (hostSyncpoint.HasSyncpointExpired(data.fence.id, data.fence.value)) {
            data.value.val = hostSyncpoint.ReadSyncpointMinValue(data.fence.id);
            return NvStatus::Success;
        }

        // Sync the syncpoint with the GPU then check again
        auto minVal = hostSyncpoint.UpdateMin(data.fence.id);
        if (hostSyncpoint.HasSyncpointExpired(data.fence.id, data.fence.value)) {
            data.value.val = minVal;
            return NvStatus::Success;
        }

        u32 userEventId{};
        if (async) {
            if (data.value.val >= constant::NvHostEventCount || !events.at(data.value.val))
                return NvStatus::BadValue;

            userEventId = data.value.val;
        } else {
            data.fence.value = 0;

            userEventId = FindFreeEvent(data.fence.id);
        }

        auto& event = *events.at(userEventId);
        if (event.state == NvHostEvent::State::Cancelled || event.state == NvHostEvent::State::Available || event.state == NvHostEvent::State::Signaled) {
            state.logger->Debug("Now waiting on nvhost event: {} with fence: {}", userEventId, data.fence.id);
            event.Wait(state.gpu, data.fence);

            data.value.val = 0;

            if (async) {
                data.value.syncpointIdAsync = data.fence.id;
            } else {
                data.value.syncpointIdNonAsync = data.fence.id;
                data.value.nonAsync = true;
            }

            data.value.val |= userEventId;

            return NvStatus::Timeout;
        } else {
            return NvStatus::BadValue;
        }
    }

    NvStatus NvHostCtrl::GetConfig(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return NvStatus::BadValue;
    }

    NvStatus NvHostCtrl::EventSignal(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        auto userEventId{buffer.as<u32>()};
        state.logger->Debug("Signalling nvhost event: {}", userEventId);

        if (userEventId >= constant::NvHostEventCount || !events.at(userEventId))
            return NvStatus::BadValue;

        auto &event = *events.at(userEventId);

        if (event.state == NvHostEvent::State::Waiting) {
            event.state = NvHostEvent::State::Cancelling;
            state.logger->Debug("Cancelling waiting nvhost event: {}", userEventId);
            event.Cancel(state.gpu);
        }

        event.state = NvHostEvent::State::Cancelled;

        auto driver = nvdrv::driver.lock();
        auto &hostSyncpoint = driver->hostSyncpoint;
        hostSyncpoint.UpdateMin(event.fence.id);

        return NvStatus::Success;
    }

    NvStatus NvHostCtrl::EventWait(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return EventWaitImpl(buffer, false);
    }

    NvStatus NvHostCtrl::EventWaitAsync(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        return EventWaitImpl(buffer, true);
    }

    NvStatus NvHostCtrl::EventRegister(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
        auto userEventId{buffer.as<u32>()};
        state.logger->Debug("Registering nvhost event: {}", userEventId);

        auto &event = events.at(userEventId);
        if (event)
            throw exception("Recreating events is unimplemented");
        event = NvHostEvent(state);

        return NvStatus::Success;
    }

    std::shared_ptr<type::KEvent> NvHostCtrl::QueryEvent(u32 eventId) {
        EventValue eventValue{.val = eventId};
        const auto &event = events.at(eventValue.nonAsync ? eventValue.eventSlotNonAsync : eventValue.eventSlotAsync);

        if (event && event->fence.id == (eventValue.nonAsync ? eventValue.syncpointIdNonAsync : eventValue.syncpointIdAsync))
            return event->event;

        return nullptr;
    }
}
