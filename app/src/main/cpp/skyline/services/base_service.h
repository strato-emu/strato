#pragma once

#include <functional>
#include <kernel/ipc.h>
#include <common.h>

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
        audout_u,
        audout_IAudioOut,
        IAudioRendererManager,
        IAudioRenderer,
        hid,
        hid_IAppletResource,
        timesrv_IStaticService,
        timesrv_ISystemClock,
        timesrv_ITimeZoneService,
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
        {"fatal:u", Service::fatal_u},
        {"set:sys", Service::set_sys},
        {"apm", Service::apm},
        {"appletOE", Service::am_appletOE},
        {"appletAE", Service::am_appletAE},
        {"audout:u", Service::audout_u},
        {"audren:u", Service::IAudioRendererManager},
        {"hid", Service::hid},
        {"time:s", Service::timesrv_IStaticService},
        {"time:a", Service::timesrv_IStaticService},
        {"time:u", Service::timesrv_IStaticService},
        {"fsp-srv", Service::fs_fsp},
        {"nvdrv", Service::nvdrv},
        {"nvdrv:a", Service::nvdrv},
        {"nvdrv:s", Service::nvdrv},
        {"nvdrv:t", Service::nvdrv},
        {"vi:m", Service::vi_m},
    };

    class ServiceManager;

    /**
     * @brief The BaseService class is a class for all Services to inherit from
     */
    class BaseService {
      protected:
        const DeviceState &state; //!< The state of the device
        ServiceManager &manager; //!< A pointer to the service manager
        const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> vTable; //!< This holds the mapping from an object's CmdId to the actual function

      public:
        const Service serviceType; //!< The type of the service this is
        const std::string serviceName; //!< The name of the service

        /**
         * @param state The state of the device
         * @param hasLoop If the service has a loop or not
         * @param serviceType The type of the service
         * @param serviceName The name of the service
         * @param vTable The functions of the service
         */
        BaseService(const DeviceState &state, ServiceManager &manager, const Service serviceType, const std::string &serviceName, const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> &vTable) : state(state), manager(manager), serviceType(serviceType), serviceName(serviceName), vTable(vTable) {}

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
                state.logger->Warn("Cannot find function in service '{0}' (Type: {1}): 0x{2:X} ({2})", serviceName, serviceType, static_cast<u32>(request.payload->value));
                return;
            }
            try {
                function(session, request, response);
            } catch (std::exception &e) {
                throw exception("{} (Service: {})", e.what(), serviceName);
            }
        };
    };
}
