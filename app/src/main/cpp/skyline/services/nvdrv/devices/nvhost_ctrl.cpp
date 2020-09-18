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

    void NvHostCtrl::EventWaitImpl(IoctlData &buffer, bool async) {
        struct Data {
            Fence fence;
            u32 timeout;
            EventValue value;
        } &args = state.process->GetReference<Data>(buffer.output.at(0).address);

        if (args.fence.id >= constant::MaxHwSyncpointCount) {
            buffer.status = NvStatus::BadValue;
            return;
        }

        if (args.timeout == 0) {
            buffer.status = NvStatus::Timeout;
            return;
        }

        auto driver = nvdrv::driver.lock();
        auto &hostSyncpoint = driver->hostSyncpoint;

        // Check if the syncpoint has already expired using the last known values
        if (hostSyncpoint.HasSyncpointExpired(args.fence.id, args.fence.value)) {
            args.value.val = hostSyncpoint.ReadSyncpointMinValue(args.fence.id);
            return;
        }

        // Sync the syncpoint with the GPU then check again
        auto minVal = hostSyncpoint.UpdateMin(args.fence.id);
        if (hostSyncpoint.HasSyncpointExpired(args.fence.id, args.fence.value)) {
            args.value.val = minVal;
            return;
        }

        u32 userEventId{};

        if (async) {
            if (args.value.val >= constant::NvHostEventCount || !events.at(args.value.val)) {
                buffer.status = NvStatus::BadValue;
                return;
            }

            userEventId = args.value.val;
        } else {
            args.fence.value = 0;

            userEventId = FindFreeEvent(args.fence.id);
        }

        auto event = &*events.at(userEventId);

        if (event->state == NvHostEvent::State::Cancelled || event->state == NvHostEvent::State::Available || event->state == NvHostEvent::State::Signaled) {
            state.logger->Debug("Now waiting on nvhost event: {} with fence: {}", userEventId, args.fence.id);
            event->Wait(state.gpu, args.fence);

            args.value.val = 0;

            if (async) {
                args.value.syncpointIdAsync = args.fence.id;
            } else {
                args.value.syncpointIdNonAsync = args.fence.id;
                args.value.nonAsync = true;
            }

            args.value.val |= userEventId;

            buffer.status = NvStatus::Timeout;
            return;
        } else {
            buffer.status = NvStatus::BadValue;
            return;
        }
    }

    void NvHostCtrl::GetConfig(IoctlData &buffer) {
        buffer.status = NvStatus::BadValue;
    }

    void NvHostCtrl::EventSignal(IoctlData &buffer) {
        auto userEventId = static_cast<u16>(state.process->GetObject<u32>(buffer.input.at(0).address));
        state.logger->Debug("Signalling nvhost event: {}", userEventId);

        if (userEventId >= constant::NvHostEventCount || !events.at(userEventId)) {
            buffer.status = NvStatus::BadValue;
            return;
        }

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
    }

    void NvHostCtrl::EventWait(IoctlData &buffer) {
        EventWaitImpl(buffer, false);
    }

    void NvHostCtrl::EventWaitAsync(IoctlData &buffer) {
        EventWaitImpl(buffer, true);
    }

    void NvHostCtrl::EventRegister(IoctlData &buffer) {
        auto userEventId = state.process->GetObject<u32>(buffer.input.at(0).address);
        state.logger->Debug("Registering nvhost event: {}", userEventId);

        auto &event = events.at(userEventId);

        if (event)
            throw exception("Recreating events is unimplemented");

        event = NvHostEvent(state);
    }

    std::shared_ptr<type::KEvent> NvHostCtrl::QueryEvent(u32 eventId) {
        auto eventValue = EventValue{.val = eventId};
        const auto &event = events.at(eventValue.nonAsync ? eventValue.eventSlotNonAsync : eventValue.eventSlotAsync);

        if (event && event->fence.id == (eventValue.nonAsync ? eventValue.syncpointIdNonAsync : eventValue.syncpointIdAsync))
            return event->event;

        return nullptr;
    }
}
