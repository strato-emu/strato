// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <services/timesrv/results.h>
#include "ITimeZoneService.h"
#include "IStaticService.h"

namespace skyline::service::glue {
    IStaticService::IStaticService(const DeviceState &state, ServiceManager &manager, std::shared_ptr<timesrv::IStaticService> core, timesrv::core::TimeServiceObject &timesrvCore, timesrv::StaticServicePermissions permissions) : BaseService(state, manager), core(std::move(core)), timesrvCore(timesrvCore), permissions(permissions) {}

    Result IStaticService::GetStandardUserSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->GetStandardUserSystemClock(session, request, response);
    }

    Result IStaticService::GetStandardNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->GetStandardNetworkSystemClock(session, request, response);
    }

    Result IStaticService::GetStandardSteadyClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->GetStandardSteadyClock(session, request, response);
    }

    Result IStaticService::GetTimeZoneService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ITimeZoneService>(state, manager, core->GetTimeZoneService(state, manager), timesrvCore, true), session, response);
        return {};
    }

    Result IStaticService::GetStandardLocalSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->GetStandardLocalSystemClock(session, request, response);
    }

    Result IStaticService::GetEphemeralNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->GetEphemeralNetworkSystemClock(session, request, response);
    }

    Result IStaticService::GetSharedMemoryNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->GetSharedMemoryNativeHandle(session, request, response);
    }

    Result IStaticService::SetStandardSteadyClockInternalOffset(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!permissions.writeSteadyClock)
            return timesrv::result::PermissionDenied;

        // HOS would write the offset between the RTC and the epoch here, however as we emulate an RTC with no offset we can ignore this
        return {};
    }

    Result IStaticService::GetStandardSteadyClockRtcValue(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // std::time is effectively our RTC
        response.Push<timesrv::PosixTime>(std::time(nullptr));
        return {};
    }

    Result IStaticService::IsStandardUserSystemClockAutomaticCorrectionEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->IsStandardUserSystemClockAutomaticCorrectionEnabled(session, request, response);
    }

    Result IStaticService::SetStandardUserSystemClockAutomaticCorrectionEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->SetStandardUserSystemClockAutomaticCorrectionEnabled(session, request, response);
    }

    Result IStaticService::GetStandardUserSystemClockInitialYear(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        constexpr i32 standardUserSystemClockInitialYear{2019}; //!< https://switchbrew.org/wiki/System_Settings#time
        response.Push(standardUserSystemClockInitialYear);
        return {};
    }

    Result IStaticService::IsStandardNetworkSystemClockAccuracySufficient(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->IsStandardNetworkSystemClockAccuracySufficient(session, request, response);
    }

    Result IStaticService::GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(session, request, response);
    }

    Result IStaticService::CalculateMonotonicSystemClockBaseTimePoint(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->CalculateMonotonicSystemClockBaseTimePoint(session, request, response);
    }

    Result IStaticService::GetClockSnapshot(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->GetClockSnapshot(session, request, response);
    }

    Result IStaticService::GetClockSnapshotFromSystemClockContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->GetClockSnapshotFromSystemClockContext(session, request, response);
    }

    Result IStaticService::CalculateStandardUserSystemClockDifferenceByUser(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->CalculateStandardUserSystemClockDifferenceByUser(session, request, response);
    }

    Result IStaticService::CalculateSpanBetween(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->CalculateSpanBetween(session, request, response);
    }
}
