// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::timesrv {
    namespace core {
        struct TimeServiceObject;
    }

    /**
     * @brief ITimeZoneService is used to retrieve and set timezone info and convert between times and dates by the system
     * @url https://switchbrew.org/wiki/PSC_services#ITimeZoneService
     */
    class ITimeZoneService : public BaseService {
      private:
        core::TimeServiceObject &core;
        bool writeable; //!< If this instance is allowed to set the device timezone

      public:
        ITimeZoneService(const DeviceState &state, ServiceManager &manager, core::TimeServiceObject &core, bool writeable);

        Result GetDeviceLocationName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetDeviceLocationName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetTotalLocationNameCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result LoadLocationNameList(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result LoadTimeZoneRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetTimeZoneRuleVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetDeviceLocationNameAndUpdatedTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetDeviceLocationNameWithTimeZoneBinaryIpc(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetDeviceLocationNameWithTimeZoneBinary(std::string_view locationName, span<u8> rule);

        Result ParseTimeZoneBinaryIpc(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result ParseTimeZoneBinary(span<u8> binary, span<u8> rule);

        Result GetDeviceLocationNameOperationEventReadableHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result ToCalendarTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result ToCalendarTimeWithMyRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result ToPosixTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result ToPosixTimeWithMyRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, ITimeZoneService, GetDeviceLocationName),
            SFUNC(0x1, ITimeZoneService, SetDeviceLocationName),
            SFUNC(0x2, ITimeZoneService, GetTotalLocationNameCount),
            SFUNC(0x3, ITimeZoneService, LoadLocationNameList),
            SFUNC(0x4, ITimeZoneService, LoadTimeZoneRule),
            SFUNC(0x5, ITimeZoneService, GetTimeZoneRuleVersion),
            SFUNC(0x6, ITimeZoneService, GetDeviceLocationNameAndUpdatedTime),
            SFUNC(0x7, ITimeZoneService, SetDeviceLocationNameWithTimeZoneBinaryIpc),
            SFUNC(0x8, ITimeZoneService, ParseTimeZoneBinaryIpc),
            SFUNC(0x9, ITimeZoneService, GetDeviceLocationNameOperationEventReadableHandle),
            SFUNC(0x64, ITimeZoneService, ToCalendarTime),
            SFUNC(0x65, ITimeZoneService, ToCalendarTimeWithMyRule),
            SFUNC(0xC9, ITimeZoneService, ToPosixTime),
            SFUNC(0xCA, ITimeZoneService, ToPosixTimeWithMyRule)
        )
    };
}
