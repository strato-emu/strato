// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <vfs/filesystem.h>
#include <ctime>
#include "core.h"
#include "time_manager_server.h"

namespace skyline::service::timesrv::core {
    TimeSpanType SteadyClockCore::GetRawTimePoint() {
        auto timePoint{GetTimePoint()};

        if (timePoint)
            return TimeSpanType::FromSeconds(timePoint->timePoint);
        else
            throw exception("Error reading timepoint");
    }

    ResultValue<SteadyClockTimePoint> SteadyClockCore::GetCurrentTimePoint() {
        auto timePoint{GetTimePoint()};
        if (timePoint)
            timePoint->timePoint += (GetTestOffset() + GetInternalOffset()).Seconds();

        return timePoint;
    }

    TimeSpanType SteadyClockCore::GetCurrentRawTimePoint() {
        return GetRawTimePoint() + GetTestOffset() + GetInternalOffset();
    }

    void StandardSteadyClockCore::Setup(UUID pRtcId, TimeSpanType pRtcOffset, TimeSpanType pInternalOffset, TimeSpanType pTestOffset, bool rtcResetDetected) {
        rtcId = pRtcId;
        rtcOffset = pRtcOffset;
        internalOffset = pInternalOffset;
        testOffset = pTestOffset;

        if (rtcResetDetected)
            SetRtcReset();

        MarkInitialized();
    }

    ResultValue<SteadyClockTimePoint> StandardSteadyClockCore::GetTimePoint() {
        SteadyClockTimePoint timePoint{
            .timePoint = GetRawTimePoint().Seconds(),
            .clockSourceId = rtcId,
        };

        return timePoint;
    }

    TimeSpanType StandardSteadyClockCore::GetRawTimePoint() {
        std::lock_guard lock(mutex);

        auto timePoint{TimeSpanType::FromNanoseconds(util::GetTimeNs()) + rtcOffset};
        if (timePoint > cachedValue)
            cachedValue = timePoint;

        return timePoint;
    }

    ResultValue<SteadyClockTimePoint> TickBasedSteadyClockCore::GetTimePoint() {
        SteadyClockTimePoint timePoint{
            .timePoint = TimeSpanType::FromNanoseconds(util::GetTimeNs()).Seconds(),
            .clockSourceId = id,
        };

        return timePoint;
    }

    bool SystemClockCore::IsClockSetup() {
        if (GetClockContext()) {
            auto timePoint{steadyClock.GetCurrentTimePoint()};
            if (timePoint)
                return timePoint->clockSourceId.Valid();
        }

        return false;
    }

    Result SystemClockCore::UpdateClockContext(const SystemClockContext &newContext) {
        auto result{SetClockContext(newContext)};
        if (result)
            return result;

        // Writes new state to shared memory etc
        return updateCallback ? updateCallback->UpdateContext(newContext) : Result{};
    }

    Result SystemClockCore::SetCurrentTime(PosixTime posixTimePoint) {
        auto timePoint{steadyClock.GetCurrentTimePoint()};
        if (!timePoint)
            return timePoint;

        // Set new context with an offset relative to the given POSIX time
        SystemClockContext newContext{
            .timestamp = *timePoint,
            .offset = posixTimePoint - timePoint->timePoint,
        };

        UpdateClockContext(newContext);

        return {};
    }

    ResultValue<PosixTime> SystemClockCore::GetCurrentTime() {
        auto timePoint{steadyClock.GetCurrentTimePoint()};
        if (!timePoint)
            return timePoint;

        auto clockContext{GetClockContext()};
        if (!clockContext)
            return clockContext;

        if (clockContext->timestamp.clockSourceId != timePoint->clockSourceId)
            return result::ClockSourceIdMismatch;

        return clockContext->offset + timePoint->timePoint;
    }

    void StandardLocalSystemClockCore::Setup(const SystemClockContext &context, PosixTime posixTime) {
        auto timePoint{steadyClock.GetCurrentTimePoint()};

        Result result{};

        // If the new context comes from the same clock as what we currently have we don't need to set any offset as they share the same base
        if (timePoint && timePoint->clockSourceId == context.timestamp.clockSourceId)
            result = UpdateClockContext(context);
        else
            result = SetCurrentTime(posixTime);

        if (result)
            throw exception("Failed to setup StandardLocalSystemClockCore");

        MarkInitialized();
    }

    void StandardNetworkSystemClockCore::Setup(const SystemClockContext &context, TimeSpanType newSufficientAccuracy) {
        if (UpdateClockContext(context))
            throw exception("Failed to set up StandardNetworkSystemClockCore");

        sufficientAccuracy = newSufficientAccuracy;
        MarkInitialized();
    }

    bool StandardNetworkSystemClockCore::IsAccuracySufficient() {
        if (!IsClockInitialized())
            return false;

        auto timePoint{steadyClock.GetCurrentTimePoint()};
        if (!timePoint)
            return false;

        auto spanBetween{GetSpanBetween(context.timestamp, *timePoint)};
        return spanBetween && *spanBetween < sufficientAccuracy.Seconds();
    }

    Result StandardUserSystemClockCore::SetAutomaticCorrectionEnabled(bool enable) {
        // Resync with network clock before any state transitions
        if (enable != automaticCorrectionEnabled && networkSystemClock.IsClockSetup()) {
            auto ctx{networkSystemClock.GetClockContext()};
            if (!ctx)
                return ctx;

            auto result{localSystemClock.SetClockContext(*ctx)};
            if (result)
                return result;
        }

        automaticCorrectionEnabled = enable;
        return {};
    }

    void StandardUserSystemClockCore::SetAutomaticCorrectionUpdatedTime(const SteadyClockTimePoint &timePoint) {
        automaticCorrectionUpdatedTime = timePoint;
        automaticCorrectionUpdatedEvent->Signal();
    }

    Result StandardUserSystemClockCore::UpdateAutomaticCorrectionState(bool enable) {
        auto result{SetAutomaticCorrectionEnabled(enable)};
        if (!result) {
            timeSharedMemory.SetStandardUserSystemClockAutomaticCorrectionEnabled(enable);

            auto timePoint{steadyClock.GetCurrentTimePoint()};
            if (timePoint)
                SetAutomaticCorrectionUpdatedTime(*timePoint);
            else
                return timePoint;
        }

        return result;
    }

    void StandardUserSystemClockCore::Setup(bool enableAutomaticCorrection, const SteadyClockTimePoint &automaticCorrectionUpdateTime) {
        if (SetAutomaticCorrectionEnabled(enableAutomaticCorrection))
            throw exception("Failed to set up SetupStandardUserSystemClock: failed to set automatic correction state!");

        SetAutomaticCorrectionUpdatedTime(automaticCorrectionUpdateTime);

        MarkInitialized();

        timeSharedMemory.SetStandardUserSystemClockAutomaticCorrectionEnabled(enableAutomaticCorrection);
    }

    ResultValue<SystemClockContext> StandardUserSystemClockCore::GetClockContext() {
        if (automaticCorrectionEnabled && networkSystemClock.IsClockSetup()) {
            auto ctx{networkSystemClock.GetClockContext()};
            if (!ctx)
                return ctx;

            auto result{localSystemClock.SetClockContext(*ctx)};
            if (result)
                return result;
        }

        return localSystemClock.GetClockContext();
    }

    TimeServiceObject::TimeServiceObject(const DeviceState &state) : timeSharedMemory(state), localSystemClockContextWriter(timeSharedMemory), networkSystemClockContextWriter(timeSharedMemory), localSystemClock(standardSteadyClock), networkSystemClock(standardSteadyClock), userSystemClock(state, standardSteadyClock, localSystemClock, networkSystemClock, timeSharedMemory), empheralSystemClock(tickBasedSteadyClock), managerServer(*this) {
        // Setup time service:
        // A new rtc UUID is generated every time glue inits time
        auto rtcId{UUID::GenerateUuidV4()};
        auto rtcOffset{TimeSpanType::FromSeconds(std::time(nullptr)) - TimeSpanType::FromNanoseconds(util::GetTimeNs())};

        // On the switch the RTC may not always start from the epoch so it is compensated with the internal offset.
        // We however emulate RTC to start from the epoch so we can set it to zero, if we wanted to add an option for a system time offset we would change this.
        TimeSpanType internalOffset{};

        // Setup the standard steady clock from which everything in the system counts
        managerServer.SetupStandardSteadyClock(rtcId, rtcOffset, internalOffset, {}, false);

        SystemClockContext localSystemClockContext{
            .timestamp {
                .timePoint = 0,
                .clockSourceId = rtcId,
            },
            .offset = 0 //!< Zero offset as the RTC is calibrated already
        };
        // Don't supply a POSIX time as the offset will be taken from the above context instead.
        // Normally the POSIX time would be the initial year for the clock to reset to if the context got wiped.
        managerServer.SetupStandardLocalSystemClock(localSystemClockContext, 0);

        // Use the context just created in local clock for the network clock, HOS gets this from settings
        auto context{localSystemClock.GetClockContext()};
        if (!context)
            throw exception("Failed to get local system clock context!");

        constexpr TimeSpanType sufficientAccuracy{TimeSpanType::FromDays(30)}; //!< https://switchbrew.org/wiki/System_Settings#time

        managerServer.SetupStandardNetworkSystemClock(*context, sufficientAccuracy);

        // Initialise the user system clock with automatic correction disabled as we don't emulate the automatic correction thread
        managerServer.SetupStandardUserSystemClock(false, SteadyClockTimePoint{.clockSourceId = UUID::GenerateUuidV4()});
        managerServer.SetupEphemeralSystemClock();

        // Timezone init - normally done in glue

        // Act as if we just updated the current timezone
        auto timezoneUpdateTime{standardSteadyClock.GetTimePoint()};
        if (!timezoneUpdateTime)
            throw exception("Failed to create a timezone updated timepoint!");

        auto timeZoneBinaryListFile{state.os->assetFileSystem->OpenFile("tzdata/binaryList.txt")};
        std::vector<u8> buffer(timeZoneBinaryListFile->size);
        timeZoneBinaryListFile->Read(buffer);

        // Parse binaryList.txt into a vector
        auto prev{buffer.begin()};
        for (auto it{buffer.begin()}; it != buffer.end(); it++) {
            if (*it == '\n' && prev != it) {
                timesrv::LocationName name{};
                span(prev.base(), std::distance(prev, std::prev(it))).as_string().copy(name.data(), name.size());
                locationNameList.push_back(name);

                if (std::next(it) != buffer.end())
                    prev = std::next(it);
            }
        }

        auto timeZoneBinaryVersionFile{state.os->assetFileSystem->OpenFile("tzdata/version.txt")};
        std::array<u8, 0x10> timeZoneBinaryVersion{};
        timeZoneBinaryVersionFile->ReadUnchecked(timeZoneBinaryVersion);

        auto timeZoneBinaryFile{state.os->assetFileSystem->OpenFile(fmt::format("tzdata/zoneinfo/{}", state.os->deviceTimeZone))};
        buffer.resize(timeZoneBinaryFile->size);
        timeZoneBinaryFile->Read(buffer);

        managerServer.SetupTimeZoneManager(state.os->deviceTimeZone, *timezoneUpdateTime, locationNameList.size(), timeZoneBinaryVersion, buffer);
    }
}