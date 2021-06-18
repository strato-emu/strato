// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/common/fence.h>
#include "nvdevice.h"

namespace skyline {
    namespace constant {
        constexpr u32 NvHostEventCount{64}; //!< The maximum number of nvhost events
    }

    namespace service::nvdrv::device {
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

            std::recursive_mutex mutex; //!< Protects access to the entire event
            State state{State::Available};
            Fence fence{}; //!< The fence that is associated with this syncpoint event
            std::shared_ptr<type::KEvent> event{}; //!< Returned by 'QueryEvent'

            /**
             * @brief Removes any wait requests on a syncpoint event and resets its state
             */
            void Cancel(soc::host1x::Host1X &host1x);

            /**
             * @brief Asynchronously waits on a syncpoint event using the given fence
             */
            void Wait(soc::host1x::Host1X &host1x, const Fence &fence);
        };

        /**
         * @brief NvHostCtrl (/dev/nvhost-ctrl) is used for NvHost management and synchronisation
         * @url https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-ctrl
         */
        class NvHostCtrl : public NvDevice {
          private:
            std::mutex syncpointEventMutex;
            std::array<std::shared_ptr<SyncpointEvent>, constant::NvHostEventCount> syncpointEvents{};

            /**
             * @brief Finds a free syncpoint event for the given id
             * @return The index of the syncpoint event in the map
             */
            u32 FindFreeSyncpointEvent(u32 syncpointId);

            NvStatus SyncpointEventWaitImpl(span<u8> buffer, bool async);

          public:
            NvHostCtrl(const DeviceState &state);

            /**
             * @brief Gets the value of an nvdrv setting, it returns an error code on production switches
             * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_GET_CONFIG
             */
            NvStatus GetConfig(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

            /**
             * @brief Clears a syncpoint event
             * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_SYNCPT_CLEAR_EVENT_WAIT
             */
            NvStatus SyncpointClearEventWait(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

            /**
             * @brief Synchronously waits on a syncpoint event
             * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_SYNCPT_EVENT_WAIT
             */
            NvStatus SyncpointEventWait(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

            /**
             * @brief Asynchronously waits on a syncpoint event
             * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_SYNCPT_EVENT_WAIT_ASYNC
             */
            NvStatus SyncpointEventWaitAsync(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

            /**
             * @brief Registers a syncpoint event
             * @url https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_SYNCPT_REGISTER_EVENT
             */
            NvStatus SyncpointRegisterEvent(IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

            std::shared_ptr<type::KEvent> QueryEvent(u32 eventId) override;

            NVDEVICE_DECL(
                NVFUNC(0x001B, NvHostCtrl, GetConfig),
                NVFUNC(0x001C, NvHostCtrl, SyncpointClearEventWait),
                NVFUNC(0x001D, NvHostCtrl, SyncpointEventWait),
                NVFUNC(0x001E, NvHostCtrl, SyncpointEventWaitAsync),
                NVFUNC(0x001F, NvHostCtrl, SyncpointRegisterEvent)
            )
        };
    }
}
