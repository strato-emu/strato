#pragma once

#include <common.h>
#include <kernel/ipc.h>
#include <functional>

#define SFunc(function) std::bind(&function, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)

namespace skyline::kernel::type {
    class KSession;
}
namespace skyline::kernel::service {
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
        am_IApplicationProxy,
        am_ICommonStateGetter,
        am_IApplicationFunctions,
        am_ISelfController,
        am_IWindowController,
        am_IAudioController,
        am_IDisplayController,
        am_ILibraryAppletCreator,
        am_IDebugFunctions,
        hid,
        hid_IAppletResource,
    };

    /**
     * @brief A map from every service's name as a std::string to the corresponding serviceEnum
     */
    const static std::unordered_map<std::string, Service> ServiceString = {
        {"sm:", Service::sm},
        {"fatal:u", Service::fatal_u},
        {"set:sys", Service::set_sys},
        {"apm", Service::apm},
        {"apm:ISession", Service::apm_ISession},
        {"appletOE", Service::am_appletOE},
        {"am:IApplicationProxy", Service::am_IApplicationProxy},
        {"am:ICommonStateGetter", Service::am_ICommonStateGetter},
        {"am:ISelfController", Service::am_ISelfController},
        {"am:IWindowController", Service::am_IWindowController},
        {"am:IAudioController", Service::am_IAudioController},
        {"am:IDisplayController", Service::am_IDisplayController},
        {"am:ILibraryAppletCreator", Service::am_ILibraryAppletCreator},
        {"am:IApplicationFunctions", Service::am_IApplicationFunctions},
        {"am:IDebugFunctions", Service::am_IDebugFunctions},
        {"hid", Service::hid},
        {"hid:IAppletResource", Service::hid_IAppletResource},
    };

    class ServiceManager;

    /**
     * @brief The BaseService class is a class for all Services to inherit from
     */
    class BaseService {
      protected:
        const DeviceState &state; //!< The state of the device
        ServiceManager& manager; //!< A pointer to the service manager
        std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> vTable; //!< This holds the mapping from an object's CmdId to the actual function

      public:
        Service serviceType; //!< Which service this is
        const bool hasLoop; //<! If the service has a loop or not

        /**
         * @param state The state of the device
         * @param hasLoop If the service has a loop or not
         * @param serviceType The type of the service
         * @param serviceName The name of the service
         * @param vTable The functions of the service
         */
        BaseService(const DeviceState &state, ServiceManager& manager, bool hasLoop, Service serviceType, const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> &vTable) : state(state), manager(manager), hasLoop(hasLoop), serviceType(serviceType), vTable(vTable) {}

        std::string getName() {
            std::string serviceName = "";
            for (const auto& [name, type] : ServiceString)
                if(type == serviceType)
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
            } catch (std::out_of_range&) {
                state.logger->Write(Logger::Warn, "Cannot find function in service '{0}' (Type: {1}): 0x{2:X} ({2})", getName(), serviceType, u32(request.payload->value));
                return;
            }
            try {
                function(session, request, response);
            } catch (std::exception& e) {
                throw exception(e.what());
            }
        };

        /**
         * @brief This is used by some services when they need to run some code at regular intervals
         */
        virtual void Loop() {};
    };
}
