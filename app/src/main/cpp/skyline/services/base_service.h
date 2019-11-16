#pragma once

#include <common.h>
#include <kernel/ipc.h>
#include <functional>

#define SFUNC(function) std::bind(&function, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
#define SRVREG(class) std::make_shared<class>(state, manager)

namespace skyline::kernel::type {
    class KSession;
}
namespace skyline::service {
    using namespace kernel;
    /**
     * @brief This contains an enum for every service that's present
     */
    enum class Service {
        sm,
        fatal_u,
        set_sys,
        apm,
        apm_ISession,
        am_appletOE,
        am_appletAE,
        am_IApplicationProxy,
        am_ILibraryAppletProxy,
        am_ISystemAppletProxy,
        am_IOverlayAppletProxy,
        am_ICommonStateGetter,
        am_IApplicationFunctions,
        am_ISelfController,
        am_IWindowController,
        am_IAudioController,
        am_IDisplayController,
        am_ILibraryAppletCreator,
        am_IDebugFunctions,
        am_IAppletCommonFunctions,
        hid,
        hid_IAppletResource,
        time,
        time_ISystemClock,
        time_ITimeZoneService,
        fs_fsp,
        fs_IFileSystem,
        nvdrv,
        vi_m,
        vi_IApplicationDisplayService,
        vi_ISystemDisplayService,
        vi_IManagerDisplayService,
        nvnflinger_dispdrv,
    };

    /**
     * @brief A map from every service's name as a std::string to the corresponding serviceEnum
     */
    const static std::unordered_map<std::string, Service> ServiceString{
        {"sm:", Service::sm},
        {"fatal:u", Service::fatal_u},
        {"set:sys", Service::set_sys},
        {"apm", Service::apm},
        {"apm:ISession", Service::apm_ISession},
        {"appletOE", Service::am_appletOE},
        {"appletAE", Service::am_appletAE},
        {"am:IApplicationProxy", Service::am_IApplicationProxy},
        {"am:ILibraryAppletProxy", Service::am_ILibraryAppletProxy},
        {"am:ISystemAppletProxy", Service::am_ISystemAppletProxy},
        {"am:IOverlayAppletProxy", Service::am_IOverlayAppletProxy},
        {"am:ICommonStateGetter", Service::am_ICommonStateGetter},
        {"am:ISelfController", Service::am_ISelfController},
        {"am:IWindowController", Service::am_IWindowController},
        {"am:IAudioController", Service::am_IAudioController},
        {"am:IDisplayController", Service::am_IDisplayController},
        {"am:ILibraryAppletCreator", Service::am_ILibraryAppletCreator},
        {"am:IApplicationFunctions", Service::am_IApplicationFunctions},
        {"am:IDebugFunctions", Service::am_IDebugFunctions},
        {"am:IAppletCommonFunctions", Service::am_IAppletCommonFunctions},
        {"hid", Service::hid},
        {"hid:IAppletResource", Service::hid_IAppletResource},
        {"time:s", Service::time},
        {"time:a", Service::time},
        {"time:ISystemClock", Service::time_ISystemClock},
        {"time:ITimeZoneService", Service::time_ITimeZoneService},
        {"fsp-srv", Service::fs_fsp},
        {"fs:IFileSystem", Service::fs_IFileSystem},
        {"nvdrv", Service::nvdrv},
        {"nvdrv:a", Service::nvdrv},
        {"nvdrv:s", Service::nvdrv},
        {"nvdrv:t", Service::nvdrv},
        {"vi:m", Service::vi_m},
        {"vi:IApplicationDisplayService", Service::vi_IApplicationDisplayService},
        {"vi:ISystemDisplayService", Service::vi_ISystemDisplayService},
        {"vi:IManagerDisplayService", Service::vi_IManagerDisplayService},
        {"nvnflinger:dispdrv", Service::nvnflinger_dispdrv},
    };

    class ServiceManager;

    /**
     * @brief The BaseService class is a class for all Services to inherit from
     */
    class BaseService {
      protected:
        const DeviceState &state; //!< The state of the device
        ServiceManager &manager; //!< A pointer to the service manager
        std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> vTable; //!< This holds the mapping from an object's CmdId to the actual function

      public:
        Service serviceType; //!< The type of the service this is
        const bool hasLoop; //<! If the service has a loop or not

        /**
         * @param state The state of the device
         * @param hasLoop If the service has a loop or not
         * @param serviceType The type of the service
         * @param serviceName The name of the service
         * @param vTable The functions of the service
         */
        BaseService(const DeviceState &state, ServiceManager &manager, bool hasLoop, Service serviceType, const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> &vTable) : state(state), manager(manager), hasLoop(hasLoop), serviceType(serviceType), vTable(vTable) {}

        /**
         * @brief This returns the name of the current service
         * @note It may not return the exact name the service was initialized with if there are multiple entries in ServiceString
         * @return The name of the service
         */
        std::string getName() {
            std::string serviceName;
            for (const auto&[name, type] : ServiceString)
                if (type == serviceType)
                    serviceName = name;
            return serviceName;
        }

        /**
         * @brief This handles all IPC commands with type request to a service
         * @param request The corresponding IpcRequest object
         * @param response The corresponding IpcResponse object
         */
        void HandleRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
            std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)> function;
            try {
                function = vTable.at(request.payload->value);
            } catch (std::out_of_range &) {
                state.logger->Warn("Cannot find function in service '{0}' (Type: {1}): 0x{2:X} ({2})", getName(), serviceType, u32(request.payload->value));
                return;
            }
            try {
                function(session, request, response);
            } catch (std::exception &e) {
                throw exception("{} (Service: {})", e.what(), getName());
            }
        };

        /**
         * @brief This is used by some services when they need to run some code at regular intervals
         */
        virtual void Loop() {};
    };
}
