// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <kernel/types/KProcess.h>
#include <services/timesrv/common.h>
#include <services/timesrv/results.h>
#include <services/timesrv/ITimeZoneService.h>
#include <services/timesrv/core.h>
#include "ITimeZoneService.h"

namespace skyline::service::glue {
    ITimeZoneService::ITimeZoneService(const DeviceState &state, ServiceManager &manager, std::shared_ptr<timesrv::ITimeZoneService> core, timesrv::core::TimeServiceObject &timesrvCore, bool writeable) : BaseService(state, manager), core(std::move(core)), timesrvCore(timesrvCore), locationNameUpdateEvent(std::make_shared<kernel::type::KEvent>(state, false)), writeable(writeable) {}

    Result ITimeZoneService::GetDeviceLocationName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->GetDeviceLocationName(session, request, response);
    }

    Result ITimeZoneService::SetDeviceLocationName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!writeable)
            return timesrv::result::PermissionDenied;

        auto locationName{span(request.Pop<timesrv::LocationName>()).as_string(true)};
        auto timeZoneBinaryFile{state.os->assetFileSystem->OpenFile(fmt::format("tzdata/zoneinfo/{}", locationName))};
        std::vector<u8> timeZoneBinaryBuffer(timeZoneBinaryFile->size);
        timeZoneBinaryFile->Read(timeZoneBinaryBuffer);
        auto result{core->SetDeviceLocationNameWithTimeZoneBinary(span(locationName).as_string(true), timeZoneBinaryBuffer)};
        if (result)
            return result;

        locationNameUpdateEvent->Signal();
        return {};
    }

    Result ITimeZoneService::GetTotalLocationNameCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->GetTotalLocationNameCount(session, request, response);
    }

    Result ITimeZoneService::LoadLocationNameList(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto outList{request.outputBuf.at(0).cast<timesrv::LocationName>()};
        auto offset{request.Pop<u32>()};

        outList.copy_from(span(timesrvCore.locationNameList).subspan(offset, outList.size()));

        response.Push<u32>(outList.size());
        return {};
    }

    Result ITimeZoneService::LoadTimeZoneRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto locationName{span(request.Pop<timesrv::LocationName>()).as_string(true)};
        auto timeZoneBinaryFile{state.os->assetFileSystem->OpenFile(fmt::format("tzdata/zoneinfo/{}", locationName))};
        std::vector<u8> timeZoneBinaryBuffer(timeZoneBinaryFile->size);
        timeZoneBinaryFile->Read(timeZoneBinaryBuffer);
        return core->ParseTimeZoneBinary(timeZoneBinaryBuffer, request.outputBuf.at(0));
    }

    Result ITimeZoneService::GetTimeZoneRuleVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->GetTimeZoneRuleVersion(session, request, response);
    }

    Result ITimeZoneService::GetDeviceLocationNameAndUpdatedTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return timesrv::result::Unimplemented;
    }

    Result ITimeZoneService::SetDeviceLocationNameWithTimeZoneBinary(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!writeable)
            return timesrv::result::PermissionDenied;

        return timesrv::result::Unimplemented;
    }

    Result ITimeZoneService::ParseTimeZoneBinary(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return timesrv::result::Unimplemented;
    }

    Result ITimeZoneService::GetDeviceLocationNameOperationEventReadableHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(locationNameUpdateEvent)};
        state.logger->Debug("Location Name Update Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result ITimeZoneService::ToCalendarTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->ToCalendarTime(session, request, response);
    }

    Result ITimeZoneService::ToCalendarTimeWithMyRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->ToCalendarTimeWithMyRule(session, request, response);
    }

    Result ITimeZoneService::ToPosixTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->ToPosixTime(session, request, response);
    }

    Result ITimeZoneService::ToPosixTimeWithMyRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return core->ToPosixTimeWithMyRule(session, request, response);
    }
}
