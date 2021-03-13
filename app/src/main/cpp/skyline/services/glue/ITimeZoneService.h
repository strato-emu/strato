// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/serviceman.h>

namespace skyline::service::timesrv {
    class ITimeZoneService;
    namespace core {
        struct TimeServiceObject;
    }
}

namespace skyline::service::glue {
    /**
     * @brief ITimeZoneService is glue's extension of psc::ITimeZoneService, it adds support for reading TimeZone location data and simplifies rule handling. This is the variant normally used by applications.
     * @url https://switchbrew.org/wiki/Glue_services#ITimeZoneService
     */
    class ITimeZoneService : public BaseService {
      private:
        std::shared_ptr<timesrv::ITimeZoneService> core;
        timesrv::core::TimeServiceObject &timesrvCore;
        std::shared_ptr<kernel::type::KEvent> locationNameUpdateEvent; //!< N uses a list here but a single event should be fine
        bool writeable; //!< If this instance is allowed to set the device timezone

      public:
        ITimeZoneService(const DeviceState &state, ServiceManager &manager, std::shared_ptr<timesrv::ITimeZoneService> core, timesrv::core::TimeServiceObject &timesrvCore, bool writeable);

        Result GetDeviceLocationName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetDeviceLocationName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetTotalLocationNameCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a list of available timezone location names beginning from the given index
         * @url https://switchbrew.org/wiki/Glue_services#LoadLocationNameList
         */
        Result LoadLocationNameList(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result LoadTimeZoneRule(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetTimeZoneRuleVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetDeviceLocationNameAndUpdatedTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetDeviceLocationNameWithTimeZoneBinary(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result ParseTimeZoneBinary(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

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
            SFUNC(0x7, ITimeZoneService, SetDeviceLocationNameWithTimeZoneBinary),
            SFUNC(0x8, ITimeZoneService, ParseTimeZoneBinary),
            SFUNC(0x9, ITimeZoneService, GetDeviceLocationNameOperationEventReadableHandle),
            SFUNC(0x64, ITimeZoneService, ToCalendarTime),
            SFUNC(0x65, ITimeZoneService, ToCalendarTimeWithMyRule),
            SFUNC(0xC9, ITimeZoneService, ToPosixTime),
            SFUNC(0xCA, ITimeZoneService, ToPosixTimeWithMyRule)
        )
    };
}
