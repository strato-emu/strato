// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

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
        sm_IUserInterface,
        fatalsrv_IService,
        settings_ISettingsServer,
        settings_ISystemSettingsServer,
        apm_IManager,
        apm_ISession,
        am_IAllSystemAppletProxiesService,
        am_IApplicationProxyService,
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
        am_IStorage,
        am_IStorageAccessor,
        audio_IAudioOutManager,
        audio_IAudioOut,
        audio_IAudioRendererManager,
        audio_IAudioRenderer,
        hid_IHidServer,
        hid_IAppletResource,
        timesrv_IStaticService,
        timesrv_ISystemClock,
        timesrv_ITimeZoneService,
        fssrv_IFileSystemProxy,
        fssrv_IFileSystem,
        fssrv_IStorage,
        nvdrv_INvDrvServices,
        visrv_IManagerRootService,
        visrv_IApplicationDisplayService,
        visrv_ISystemDisplayService,
        visrv_IManagerDisplayService,
        hosbinder_IHOSBinderDriver,
        pl_IPlatformServiceManager,
        aocsrv_IAddOnContentManager,
        pctl_IParentalControlServiceFactory,
        lm_ILogService,
        lm_ILogger,
        account_IAccountServiceForApplication,
    };

    /**
     * @brief A map from every service's name as a std::string to the corresponding serviceEnum
     */
    const static std::unordered_map<std::string, Service> ServiceString{
        {"fatal:u", Service::fatalsrv_IService},
        {"set", Service::settings_ISettingsServer},
        {"set:sys", Service::settings_ISystemSettingsServer},
        {"apm", Service::apm_IManager},
        {"appletOE", Service::am_IApplicationProxyService},
        {"appletAE", Service::am_IAllSystemAppletProxiesService},
        {"audout:u", Service::audio_IAudioOutManager},
        {"audren:u", Service::audio_IAudioRendererManager},
        {"hid", Service::hid_IHidServer},
        {"time:s", Service::timesrv_IStaticService},
        {"time:a", Service::timesrv_IStaticService},
        {"time:u", Service::timesrv_IStaticService},
        {"fsp-srv", Service::fssrv_IFileSystemProxy},
        {"nvdrv", Service::nvdrv_INvDrvServices},
        {"nvdrv:a", Service::nvdrv_INvDrvServices},
        {"nvdrv:s", Service::nvdrv_INvDrvServices},
        {"nvdrv:t", Service::nvdrv_INvDrvServices},
        {"vi:m", Service::visrv_IManagerRootService},
        {"pl:u", Service::pl_IPlatformServiceManager},
        {"aoc:u", Service::aocsrv_IAddOnContentManager},
        {"pctl", Service::pctl_IParentalControlServiceFactory},
        {"pctl:a", Service::pctl_IParentalControlServiceFactory},
        {"pctl:s", Service::pctl_IParentalControlServiceFactory},
        {"pctl:r", Service::pctl_IParentalControlServiceFactory},
        {"lm", Service::lm_ILogService},
        {"acc:u0", Service::account_IAccountServiceForApplication},
    };

    class ServiceManager;

    /**
     * @brief The BaseService class is a class for all Services to inherit from
     */
    class BaseService {
      protected:
        const DeviceState &state; //!< The state of the device
        ServiceManager &manager; //!< A reference to the service manager
        std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> vTable; //!< This holds the mapping from a function's CmdId to the actual function

      public:
        Service serviceType; //!< The type of this service
        std::string serviceName; //!< The name of this service

        /**
         * @param state The state of the device
         * @param hasLoop If the service has a loop or not
         * @param serviceType The type of the service
         * @param serviceName The name of the service
         * @param vTable The functions of the service
         */
        BaseService(const DeviceState &state, ServiceManager &manager, Service serviceType, const std::string &serviceName, const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> &vTable) : state(state), manager(manager), serviceType(serviceType), serviceName(serviceName), vTable(vTable) {}

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
