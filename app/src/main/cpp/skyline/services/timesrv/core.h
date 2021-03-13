// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <common/uuid.h>
#include <kernel/types/KEvent.h>
#include <kernel/types/KSharedMemory.h>
#include <services/timesrv/common.h>
#include <horizon_time.h>
#include "time_shared_memory.h"
#include "time_manager_server.h"
#include "timezone_manager.h"
#include "results.h"

namespace skyline::service::timesrv::core {
    /**
     * @brief A steady clock provides a monotonically increasing timepoint calibrated from a specific base
     */
    class SteadyClockCore {
        bool rtcResetDetected{}; //!< True if the RTC this clock is based off has reset before this boot.
        bool initialized{}; //!< If this clock is calibrated with offsets etc and ready for use by applications

      protected:
        void SetRtcReset() {
            rtcResetDetected = true;
        }

        void MarkInitialized() {
            initialized = true;
        }

      public:
        bool IsRtcResetDetected() {
            return rtcResetDetected;
        }

        bool IsClockInitialized() {
            return initialized;
        }

        /**
         * @brief Returns the current timepoint of the clock including offsets in a SteadyClockTimePoint struct with a source UUID
         */
        ResultValue<SteadyClockTimePoint> GetCurrentTimePoint();

        /**
         * @brief Returns the current raw timepoint of the clock including offsets but without any UUID, this may have higher accuracy
         */
        TimeSpanType GetCurrentRawTimePoint();

        /**
         * @brief Returns the base timepoint of the clock without any offsets applied in a SteadyClockTimePoint struct with a source UUID
         */
        virtual ResultValue<SteadyClockTimePoint> GetTimePoint() = 0;

        /**
         * @brief Returns the current raw timepoint of the clock without any offsets applied without any UUID, this may have higher accuracy
         */
        virtual TimeSpanType GetRawTimePoint();

        /**
         * @brief A test offset is used to alter the base timepoint of the steady clock without it being visible to applications
         */
        virtual TimeSpanType GetTestOffset() {
            return {};
        }

        virtual void SetTestOffset(TimeSpanType offset) {}

        /**
         * @brief The internal offset is the offset between the raw steady clock time and the target time of this steady clock
         */
        virtual TimeSpanType GetInternalOffset() {
            return {};
        }

        virtual void SetInternalOffset(TimeSpanType offset) {
            return;
        }

        /**
         * @brief Returns the current value of the RTC that backs this clock
         */
        virtual ResultValue<PosixTime> GetRtcValue() {
            return result::Unimplemented;
        }

        virtual Result GetSetupResult() {
            return {};
        }
    };

    /**
     * @brief The standard steady clock is calibrated against system RTC time and is used as a base for all clocks aside from alarms and ephemeral
     */
    class StandardSteadyClockCore : public SteadyClockCore {
        std::mutex mutex; //!< Protects accesses to cachedValue
        TimeSpanType testOffset{};
        TimeSpanType internalOffset{};
        TimeSpanType rtcOffset{}; //!< The offset between the RTC timepoint and the raw timepoints of this clock
        TimeSpanType cachedValue{}; //!< Stores the cached time value, used to prevent time ever decreasing
        UUID rtcId{}; //!< UUID of the RTC this is calibrated against

      public:
        void Setup(UUID rtcId, TimeSpanType pRtcOffset, TimeSpanType pInternalOffset, TimeSpanType pTestOffset, bool rtcResetDetected);

        void SetRtcOffset(TimeSpanType offset) {
            rtcOffset = offset;
        }

        ResultValue<SteadyClockTimePoint> GetTimePoint() override;

        TimeSpanType GetRawTimePoint() override;

        TimeSpanType GetTestOffset() override {
            return testOffset;
        }

        void SetTestOffset(TimeSpanType offset) override {
            testOffset = offset;
        }

        TimeSpanType GetInternalOffset() override {
            return internalOffset;
        }

        void SetInternalOffset(TimeSpanType offset) override {
            internalOffset = offset;
        }
    };

    /**
     * @brief The tick-based steady clock provides a monotonically increasing steady clock that is based off system boot
     */
    class TickBasedSteadyClockCore : public SteadyClockCore {
      private:
        UUID id{UUID::GenerateUuidV4()};

      public:
        ResultValue<SteadyClockTimePoint> GetTimePoint() override;
    };

    class SystemClockContextUpdateCallback;

    /**
     * @brief System clocks make use of the steady clock in order to provide an adjusted POSIX timepoint that is synchronised with the network or adapted to user time optionss
     */
    class SystemClockCore {
      private:
        bool initialized{}; //!< True if the clock is safe to be used by applications and in a defined state
        SystemClockContextUpdateCallback *updateCallback; //!< Called when the context of the clock is updated

      protected:
        SteadyClockCore &steadyClock; //!< Clock that backs this system clock
        SystemClockContext context{}; //!< Holds the currently in-use context of the clock

        void MarkInitialized() {
            initialized = true;
        }

      public:
        SystemClockCore(SteadyClockCore &steadyClock) : steadyClock(steadyClock) {}

        void AddOperationEvent(const std::shared_ptr<kernel::type::KEvent> &event) {
            updateCallback->AddOperationEvent(event);
        }

        void SetUpdateCallback(SystemClockContextUpdateCallback *callback) {
            updateCallback = callback;
        }

        bool IsClockInitialized() {
            return initialized;
        }

        /**
         * @brief Checks if this system clock can produce a valid timepoint
         */
        bool IsClockSetup();

        /**
         * @brief Updates the clock to use the given context and calls the update callback
         */
        Result UpdateClockContext(const SystemClockContext &newContext);

        /**
         * @brief Sets the current clock offsets as if posixTimePoint is the current time, this updates the clock comtext so will call the callback
         */
        Result SetCurrentTime(PosixTime posixTimePoint);

        /**
         * @brief Returns the current POSIX time for this system clock
         */
        ResultValue<PosixTime> GetCurrentTime();

        virtual ResultValue<SystemClockContext> GetClockContext() {
            return context;
        }

        virtual Result SetClockContext(const SystemClockContext &newContext) {
            context = newContext;
            return {};
        }
    };

    /**
     * @brief The local system clock is a user configurable system clock based off of the system steady clock
     */
    class StandardLocalSystemClockCore : public SystemClockCore {
      public:
        StandardLocalSystemClockCore(SteadyClockCore &steadyClock) : SystemClockCore(steadyClock) {}

        void Setup(const SystemClockContext &context, PosixTime posixTime);
    };

    /**
     * @brief The network system clock is a network based system clock that is inconfigurable by the user in HOS
     */
    class StandardNetworkSystemClockCore : public SystemClockCore {
      private:
        TimeSpanType sufficientAccuracy{TimeSpanType::FromDays(10)}; //!< Maxiumum drift between the current steady time and the timestamp of the context currently in use

      public:
        StandardNetworkSystemClockCore(SteadyClockCore &steadyClock) : SystemClockCore(steadyClock) {}

        void Setup(const SystemClockContext &context, TimeSpanType newSufficientAccuracy);

        /**
         * @return If the clock accuracy is less than sufficientAccuracy
         */
        bool IsAccuracySufficient();
    };

    /**
     * @brief The standard user system clock provides an automatically corrected clock based on both local and network time, it is what should be used in most cases for time measurement
     */
    class StandardUserSystemClockCore : public SystemClockCore {
      private:
        StandardLocalSystemClockCore &localSystemClock; //!< The StandardLocalSystemClockCore this clock uses for correction
        StandardNetworkSystemClockCore &networkSystemClock; //!< The StandardNetworkSystemClockCore this clock uses for correction
        bool automaticCorrectionEnabled{}; //!< If automatic correction with the network clock should be enabled
        SteadyClockTimePoint automaticCorrectionUpdatedTime; //!< When automatic correction was last enabled
        TimeSharedMemory &timeSharedMemory; //!< Shmem reference for automatic correction state updating

        /**
         * @brief Sets automatic correction state and resyncs with network clock on changes
         */
        Result SetAutomaticCorrectionEnabled(bool enable);

        void SetAutomaticCorrectionUpdatedTime(const SteadyClockTimePoint &timePoint);

      public:
        std::shared_ptr<kernel::type::KEvent> automaticCorrectionUpdatedEvent;

        StandardUserSystemClockCore(const DeviceState &state, StandardSteadyClockCore &standardSteadyClock, StandardLocalSystemClockCore &localSystemClock, StandardNetworkSystemClockCore &networkSystemClock, TimeSharedMemory &timeSharedMemory) : SystemClockCore(standardSteadyClock), localSystemClock(localSystemClock), networkSystemClock(networkSystemClock), automaticCorrectionUpdatedEvent(std::make_shared<kernel::type::KEvent>(state, false)), timeSharedMemory(timeSharedMemory) {}

        void Setup(bool enableAutomaticCorrection, const SteadyClockTimePoint &automaticCorrectionUpdateTime);

        bool IsAutomaticCorrectionEnabled() {
            return automaticCorrectionEnabled;
        }

        SteadyClockTimePoint GetAutomaticCorrectionUpdatedTime() {
            return automaticCorrectionUpdatedTime;
        }

        /**
         * @brief Updates the automatic correction state in shared memory and this clock
         */
        Result UpdateAutomaticCorrectionState(bool enable);

        ResultValue<SystemClockContext> GetClockContext() override;

        /**
         * @brief Context is not directly settable here as it is derived from network and local clocks
         */
        Result SetClockContext(const SystemClockContext &pContext) override {
            return result::Unimplemented;
        }
    };

    /**
     * @brief The ephemeral network system clock provides a per-boot timepoint
     */
    class EphemeralNetworkSystemClockCore : public SystemClockCore {
      public:
        EphemeralNetworkSystemClockCore(SteadyClockCore &steadyClock) : SystemClockCore(steadyClock) {}

        void Setup() {
            MarkInitialized();
        }
    };

    /**
     * @brief Stores the global state of timesrv and exposes a manager interface for use by IPC
     */
    struct TimeServiceObject {
        TimeSharedMemory timeSharedMemory;

        LocalSystemClockUpdateCallback localSystemClockContextWriter;
        NetworkSystemClockUpdateCallback networkSystemClockContextWriter;
        EphemeralNetworkSystemClockUpdateCallback ephemeralNetworkSystemClockContextWriter;

        StandardSteadyClockCore standardSteadyClock;
        TickBasedSteadyClockCore tickBasedSteadyClock;
        StandardLocalSystemClockCore localSystemClock;
        StandardNetworkSystemClockCore networkSystemClock;
        StandardUserSystemClockCore userSystemClock;
        EphemeralNetworkSystemClockCore empheralSystemClock;

        TimeZoneManager timeZoneManager;
        std::vector<LocationName> locationNameList; //!< N stores in glue

        TimeManagerServer managerServer;

        /**
         * @brief Sets up all clocks with offsets based off of the current time
         */
        TimeServiceObject(const DeviceState &state);
    };
}
