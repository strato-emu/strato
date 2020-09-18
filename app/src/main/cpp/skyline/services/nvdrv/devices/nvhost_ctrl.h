// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/common/fence.h>
#include "nvdevice.h"

namespace skyline {
    namespace constant {
        constexpr u32 NvHostEventCount = 64; //!< The maximum number of nvhost events
    }

    namespace service::nvdrv::device {
        /**
         * @brief This represents a single registered event with an attached fence
         */
        class NvHostEvent {
          private:
            u64 waiterId{};

            void Signal();

          public:
            /**
             * @brief This enumerates the possible states of an event
             */
            enum class State {
                Available = 0,
                Waiting = 1,
                Cancelling = 2,
                Signaling = 3,
                Signaled = 4,
                Cancelled = 5,
            };

            NvHostEvent(const DeviceState &state);

            State state{State::Available};
            Fence fence{}; //!< The fence that is attached to this event
            std::shared_ptr<type::KEvent> event{}; //!< Returned by 'QueryEvent'

            /**
             * @brief Stops any wait requests on an event and immediately signals it
             */
            void Cancel(const std::shared_ptr<gpu::GPU> &gpuState);

            /**
             * @brief Asynchronously waits on an event using the given fence
             */
            void Wait(const std::shared_ptr<gpu::GPU> &gpuState, const Fence &fence);
        };

        /**
         * @brief NvHostCtrl (/dev/nvhost-ctrl) is used for GPU synchronization (https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-ctrl)
         */
        class NvHostCtrl : public NvDevice {
          private:
            /**
             * @brief This holds metadata about an event, it is used by QueryEvent and EventWait
             */
            union EventValue {
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
            static_assert(sizeof(EventValue) == sizeof(u32));

            std::array<std::optional<NvHostEvent>, constant::NvHostEventCount> events{};

            /**
             * @brief Finds a free event for the given syncpoint id
             * @return The index of the event in the event map
             */
            u32 FindFreeEvent(u32 syncpointId);

            void EventWaitImpl(IoctlData &buffer, bool async);

          public:
            NvHostCtrl(const DeviceState &state);

            /**
             * @brief This gets the value of an nvdrv setting, it returns an error code on production switches (https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_GET_CONFIG)
             */
            void GetConfig(IoctlData &buffer);

            /**
             * @brief This signals an NvHost event (https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_EVENT_SIGNAL)
             */
            void EventSignal(IoctlData &buffer);

            /**
             * @brief This synchronously waits on an NvHost event (https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_EVENT_WAIT)
             */
            void EventWait(IoctlData &buffer);

            /**
             * @brief This asynchronously waits on an NvHost event (https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_EVENT_WAIT_ASYNC)
             */
            void EventWaitAsync(IoctlData &buffer);

            /**
             * @brief This registers an NvHost event (https://switchbrew.org/wiki/NV_services#NVHOST_IOCTL_CTRL_EVENT_REGISTER)
             */
            void EventRegister(IoctlData &buffer);

            std::shared_ptr<type::KEvent> QueryEvent(u32 eventId);

            NVDEVICE_DECL(
                NVFUNC(0x001B, NvHostCtrl, GetConfig),
                NVFUNC(0x001C, NvHostCtrl, EventSignal),
                NVFUNC(0x001D, NvHostCtrl, EventWait),
                NVFUNC(0x001E, NvHostCtrl, EventWaitAsync),
                NVFUNC(0x001F, NvHostCtrl, EventRegister)
            )
        };
    }
}
