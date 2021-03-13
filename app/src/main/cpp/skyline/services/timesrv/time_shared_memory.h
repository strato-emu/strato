// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <common/uuid.h>
#include <kernel/types/KEvent.h>
#include <kernel/types/KSharedMemory.h>
#include <services/timesrv/common.h>

namespace skyline::service::timesrv::core {
    struct TimeSharedMemoryLayout;

    /**
     * @brief TimeSharedMemory holds context data about clocks in a double buffered format
     */
    class TimeSharedMemory {
      private:
        std::shared_ptr<kernel::type::KSharedMemory> kTimeSharedMemory;
        TimeSharedMemoryLayout *timeSharedMemory;

      public:
        TimeSharedMemory(const DeviceState &state);

        std::shared_ptr<kernel::type::KSharedMemory> GetSharedMemory() {
            return kTimeSharedMemory;
        }

        /**
         * @brief Fills in the steady clock section of shmem, the current time is subtracted from baseTimePoint to workout the offset
         */
        void SetupStandardSteadyClock(UUID rtcId, TimeSpanType baseTimePoint);

        void SetSteadyClockRawTimePoint(TimeSpanType timePoint);

        void UpdateLocalSystemClockContext(const SystemClockContext &context);

        void UpdateNetworkSystemClockContext(const SystemClockContext &context);

        void SetStandardUserSystemClockAutomaticCorrectionEnabled(bool enabled);
    };

    /**
     * @brief Base class for callbacks that run after a system clock context is updated
     */
    class SystemClockContextUpdateCallback {
      private:
        std::list<std::shared_ptr<kernel::type::KEvent>> operationEvents; //!< List of KEvents to be signalled when this callback is called
        std::mutex mutex; //!< Synchronises access to operationEvents
        std::optional<SystemClockContext> context; //!< The context that used when this callback was last called

      protected:
        /**
         * @brief Updates the base callback context with the one supplied as an argument
         * @return true if the context was updated
         */
        bool UpdateBaseContext(const SystemClockContext &newContext);

        /**
         * @brief Signals all events in the operation event list
         */
        void SignalOperationEvent();

      public:
        /**
         * @brief Adds an operation event to be siignalled on context updates
         */
        void AddOperationEvent(const std::shared_ptr<kernel::type::KEvent> &event);

        /**
         * @brief Replaces the current context with the supplied one and signals events if the context differs from the last used one
         */
        virtual Result UpdateContext(const SystemClockContext &newContext) = 0;
    };

    /**
     * @brief Update callback for the local system clock, handles writing data to shmem
     */
    class LocalSystemClockUpdateCallback : public SystemClockContextUpdateCallback {
      private:
        TimeSharedMemory &timeSharedMemory;

      public:
        LocalSystemClockUpdateCallback(TimeSharedMemory &timeSharedMemory);

        Result UpdateContext(const SystemClockContext &newContext) override;
    };

    /**
     * @brief Update callback for the network system clock, handles writing data to shmem
     */
    class NetworkSystemClockUpdateCallback : public SystemClockContextUpdateCallback {
      private:
        TimeSharedMemory &timeSharedMemory;

      public:
        NetworkSystemClockUpdateCallback(TimeSharedMemory &timeSharedMemory);

        Result UpdateContext(const SystemClockContext &newContext) override;
    };

    /**
     * @brief Update callback for the ephemeral network system clock, only handles signalling the event as there is no shmem entry for ephemeral
     */
    class EphemeralNetworkSystemClockUpdateCallback : public SystemClockContextUpdateCallback {
      public:
        Result UpdateContext(const SystemClockContext &newContext) override;
    };
}