// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/nvdrv/devices/nvdevice.h>
#include <services/common/fence.h>

namespace skyline::service::nvdrv::device::nvhost {
    /**
     * @brief nvhost::Ctrl (/dev/nvhost-ctrl) provides IOCTLs for synchronization using syncpoints
     * @url https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-ctrl
     */
    class Ctrl : public NvDevice {
      public:
        /**
         * @brief Metadata about a syncpoint event, used by QueryEvent and SyncpointEventWait
         */
        union SyncpointEventValue {
            u32 val;

            struct {
                u8 partialSlot : 4;
                u32 syncpointId : 28;
            };

            struct {
                u16 slot;
                u16 syncpointIdForAllocation : 12;
                bool eventAllocated : 1;
                u8 _pad12_ : 3;
            };
        };
        static_assert(sizeof(SyncpointEventValue) == sizeof(u32));

      private:
        /**
         * @brief Syncpoint Events are used to expose fences to the userspace, they can be waited on using an IOCTL or be converted into a native HOS KEvent object that can be waited on just like any other KEvent on the guest
         */
        class SyncpointEvent {
          private:
            soc::host1x::Syncpoint::WaiterHandle waiterHandle{};

            void Signal();

          public:
            enum class State {
                Available = 0,
                Waiting = 1,
                Cancelling = 2,
                Signalling = 3,
                Signalled = 4,
                Cancelled = 5,
            };

            SyncpointEvent(const DeviceState &state);

            std::atomic<State> state{State::Available};
            Fence fence{}; //!< The fence that is associated with this syncpoint event
            std::shared_ptr<type::KEvent> event{}; //!< Returned by 'QueryEvent'

            /**
             * @brief Removes any wait requests on a syncpoint event and resets its state
             * @note Accesses to this function for a specific event should be locked
             */
            void Cancel(soc::host1x::Host1x &host1x);

            /**
             * @brief Asynchronously waits on a syncpoint event using the given fence
             * @note Accesses to this function for a specific event should be locked
             */
            void RegisterWaiter(soc::host1x::Host1x &host1x, const Fence &fence);

            bool IsInUse();
        };

        static constexpr u32 SyncpointEventCount{64}; //!< The maximum number of nvhost syncpoint events

        std::mutex syncpointEventMutex;
        std::array<std::unique_ptr<SyncpointEvent>, SyncpointEventCount> syncpointEvents{};

        /**
         * @brief Finds a free syncpoint event for the given syncpoint ID
         * @note syncpointEventMutex MUST be locked when calling this
         * @return The free event slot
         */
        u32 FindFreeSyncpointEvent(u32 syncpointId);

        PosixResult SyncpointWaitEventImpl(In<Fence> fence, In<i32> timeout, InOut<SyncpointEventValue> value, bool allocate);

        /**
         * @brief Frees a single syncpoint event
         * @note syncpointEventMutex MUST be locked when calling this
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_SYNCPT_UNREGISTER_EVENT
         */
        PosixResult SyncpointFreeEventLocked(In<u32> slot);

      public:
        Ctrl(const DeviceState &state, Driver &driver, Core &core, const SessionContext &ctx);

        /**
         * @brief Clears a syncpoint event
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_SYNCPT_CLEAR_EVENT_WAIT
         */
        PosixResult SyncpointClearEventWait(In<SyncpointEventValue> value);

        /**
         * @brief Allocates a syncpoint event for the given syncpoint and registers as it waiting for the given fence
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_SYNCPT_WAIT_EVENT
         */
        PosixResult SyncpointWaitEvent(In<Fence> fence, In<i32> timeout, InOut<SyncpointEventValue> value);

        /**
         * @brief Waits on a specific syncpoint event and registers as it waiting for the given fence
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_SYNCPT_WAIT_EVENT_SINGLE
         */
        PosixResult SyncpointWaitEventSingle(In<Fence> fence, In<i32> timeout, InOut<SyncpointEventValue> value);

        /**
         * @brief Allocates a new syncpoint event at the given slot
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_SYNCPT_ALLOC_EVENT
         */
        PosixResult SyncpointAllocateEvent(In<u32> slot);

        /**
         * @brief Frees a single syncpoint event
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_SYNCPT_UNREGISTER_EVENT
         */
        PosixResult SyncpointFreeEvent(In<u32> slot);

        /**
         * @brief Frees a bitmask of a syncpoint events
         * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_SYNCPT_FREE_EVENTS
         */
        PosixResult SyncpointFreeEventBatch(In<u64> bitmask);

        std::shared_ptr<type::KEvent> QueryEvent(u32 slot) override;

        PosixResult Ioctl(IoctlDescriptor cmd, span<u8> buffer) override;
    };
}
