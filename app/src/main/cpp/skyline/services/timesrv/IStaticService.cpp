// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "results.h"
#include "core.h"
#include "ISteadyClock.h"
#include "ITimeZoneService.h"
#include "ISystemClock.h"
#include "IStaticService.h"

namespace skyline::service::timesrv {
    IStaticService::IStaticService(const DeviceState &state, ServiceManager &manager, core::TimeServiceObject &core, StaticServicePermissions permissions) : BaseService(state, manager), core(core), permissions(permissions) {}

    Result IStaticService::GetStandardUserSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(state, manager, core.userSystemClock, permissions.writeUserSystemClock, permissions.ignoreUninitializedChecks), session, response);
        return {};
    }

    Result IStaticService::GetStandardNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(state, manager, core.networkSystemClock, permissions.writeNetworkSystemClock, permissions.ignoreUninitializedChecks), session, response);
        return {};
    }

    Result IStaticService::GetStandardSteadyClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISteadyClock>(state, manager, core.standardSteadyClock, permissions.writeSteadyClock, permissions.ignoreUninitializedChecks), session, response);
        return {};
    }

    Result IStaticService::GetTimeZoneServiceIpc(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ITimeZoneService>(state, manager, core, permissions.writeTimezone), session, response);
        return {};
    }

    std::shared_ptr<ITimeZoneService> IStaticService::GetTimeZoneService(const DeviceState &state, ServiceManager &manager) {
        return std::make_shared<ITimeZoneService>(state, manager, core, permissions.writeTimezone);
    }

    Result IStaticService::GetStandardLocalSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(state, manager, core.localSystemClock, permissions.writeLocalSystemClock, permissions.ignoreUninitializedChecks), session, response);
        return {};
    }

    Result IStaticService::GetEphemeralNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(state, manager, core.networkSystemClock, permissions.writeNetworkSystemClock, permissions.ignoreUninitializedChecks), session, response);
        return {};
    }

    Result IStaticService::GetSharedMemoryNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto sharedMemory{core.timeSharedMemory.GetSharedMemory()};
        auto handle{state.process->InsertItem<type::KSharedMemory>(sharedMemory)};
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IStaticService::SetStandardSteadyClockInternalOffset(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (permissions.writeSteadyClock)
            return result::Unimplemented;
        else
            return result::PermissionDenied;
    }

    Result IStaticService::GetStandardSteadyClockRtcValue(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return result::Unimplemented;
    }

    Result IStaticService::IsStandardUserSystemClockAutomaticCorrectionEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!core.userSystemClock.IsClockInitialized())
            return result::ClockUninitialized;

        response.Push<u8>(core.userSystemClock.IsAutomaticCorrectionEnabled());
        return {};
    }

    Result IStaticService::SetStandardUserSystemClockAutomaticCorrectionEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!core.userSystemClock.IsClockInitialized() || !core.standardSteadyClock.IsClockInitialized())
            return result::ClockUninitialized;

        if (!permissions.writeUserSystemClock)
            return result::PermissionDenied;

        return core.userSystemClock.UpdateAutomaticCorrectionState(request.Pop<u8>());
    }

    Result IStaticService::GetStandardUserSystemClockInitialYear(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return result::Unimplemented;
    }

    Result IStaticService::IsStandardNetworkSystemClockAccuracySufficient(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(core.networkSystemClock.IsAccuracySufficient());
        return {};
    }

    Result IStaticService::GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!core.userSystemClock.IsClockInitialized())
            return result::ClockUninitialized;

        response.Push(core.userSystemClock.GetAutomaticCorrectionUpdatedTime());
        return {};
    }

    Result IStaticService::CalculateMonotonicSystemClockBaseTimePoint(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!core.standardSteadyClock.IsClockInitialized())
            return result::ClockUninitialized;

        auto timePoint{core.standardSteadyClock.GetCurrentTimePoint()};
        if (!timePoint)
            return timePoint;

        auto clockContext{request.Pop<SystemClockContext>()};
        if (clockContext.timestamp.clockSourceId != timePoint->clockSourceId)
            return result::ClockSourceIdMismatch;

        i64 baseTimePoint{timePoint->timePoint + clockContext.offset - TimeSpanType::FromNanoseconds(util::GetTimeNs()).Seconds()};
        response.Push(baseTimePoint);
        return {};
    }

    ResultValue<ClockSnapshot> IStaticService::GetClockSnapshotFromSystemClockContextImpl(const SystemClockContext &userContext, const SystemClockContext &networkContext, u8 unk) {
        ClockSnapshot out{};

        out.userContext = userContext;
        out.networkContext = networkContext;
        auto timePoint{core.standardSteadyClock.GetCurrentTimePoint()};
        if (!timePoint)
            return timePoint;

        out.steadyClockTimePoint = *timePoint;
        out.automaticCorrectionEnabled = core.userSystemClock.IsAutomaticCorrectionEnabled();

        auto locationName{core.timeZoneManager.GetLocationName()};
        if (!locationName)
            return locationName;

        out.locationName = *locationName;

        auto userPosixTime{ClockSnapshot::GetCurrentTime(out.steadyClockTimePoint, out.userContext)};
        if (!userPosixTime)
            return userPosixTime;

        out.userPosixTime = *userPosixTime;

        auto userCalendarTime{core.timeZoneManager.ToCalendarTimeWithMyRule(*userPosixTime)};
        if (!userCalendarTime)
            return userCalendarTime;

        out.userCalendarTime = userCalendarTime->calendarTime;
        out.userCalendarAdditionalInfo = userCalendarTime->additionalInfo;

        // Not necessarily a fatal error if this fails
        auto networkPosixTime{ClockSnapshot::GetCurrentTime(out.steadyClockTimePoint, out.networkContext)};
        if (networkPosixTime)
            out.networkPosixTime = *networkPosixTime;
        else
            out.networkPosixTime = 0;

        auto networkCalendarTime{core.timeZoneManager.ToCalendarTimeWithMyRule(out.networkPosixTime)};
        if (!networkCalendarTime)
            return networkCalendarTime;

        out.networkCalendarTime = networkCalendarTime->calendarTime;
        out.networkCalendarAdditionalInfo = networkCalendarTime->additionalInfo;

        out._unk_ = unk;
        out.version = 0;

        return out;
    }

    Result IStaticService::GetClockSnapshot(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto unk{request.Pop<u8>()};

        auto userContext{core.userSystemClock.GetClockContext()};
        if (!userContext)
            return userContext;

        auto networkContext{core.networkSystemClock.GetClockContext()};
        if (!networkContext)
            return networkContext;

        auto snapshot{GetClockSnapshotFromSystemClockContextImpl(*userContext, *networkContext, unk)};
        if (!snapshot)
            return snapshot;

        request.outputBuf.at(0).as<ClockSnapshot>() = *snapshot;
        return {};
    }

    Result IStaticService::GetClockSnapshotFromSystemClockContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto unk{request.Pop<u8>()};
        request.Skip<std::array<u8, 7>>();
        auto userContext{request.Pop<SystemClockContext>()};
        auto networkContext{request.Pop<SystemClockContext>()};

        auto snapshot{GetClockSnapshotFromSystemClockContextImpl(userContext, networkContext, unk)};
        if (!snapshot)
            return snapshot;

        request.outputBuf.at(0).as<ClockSnapshot>() = *snapshot;
        return {};
    }

    Result IStaticService::CalculateStandardUserSystemClockDifferenceByUser(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto snapshotA{request.inputBuf.at(0).as<ClockSnapshot>()};
        auto snapshotB{request.inputBuf.at(1).as<ClockSnapshot>()};

        TimeSpanType difference{TimeSpanType::FromSeconds(snapshotB.userContext.offset - snapshotA.userContext.offset)};

        if (snapshotA.userContext.timestamp.clockSourceId != snapshotB.userContext.timestamp.clockSourceId) {
            difference = 0;
        } else if (snapshotA.automaticCorrectionEnabled && snapshotB.automaticCorrectionEnabled) {
            if (snapshotA.networkContext.timestamp.clockSourceId != snapshotA.steadyClockTimePoint.clockSourceId || snapshotB.networkContext.timestamp.clockSourceId != snapshotB.steadyClockTimePoint.clockSourceId)
                difference = 0;
        }

        response.Push<i64>(difference.Nanoseconds());
        return {};
    }

    Result IStaticService::CalculateSpanBetween(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto snapshotA{request.inputBuf.at(0).as<ClockSnapshot>()};
        auto snapshotB{request.inputBuf.at(1).as<ClockSnapshot>()};

        auto difference{GetSpanBetween(snapshotA.steadyClockTimePoint, snapshotB.steadyClockTimePoint)};
        if (difference) {
            response.Push(TimeSpanType::FromSeconds(*difference).Nanoseconds());
            return difference;
        }

        // If GetSpanBetween fails then fall back to comparing POSIX timepoints
        if (snapshotA.networkPosixTime && snapshotB.networkPosixTime) {
            response.Push(TimeSpanType::FromSeconds(snapshotB.networkPosixTime - snapshotA.networkPosixTime).Nanoseconds());
            return {};
        } else {
            return result::InvalidComparison;
        }
    }
}
