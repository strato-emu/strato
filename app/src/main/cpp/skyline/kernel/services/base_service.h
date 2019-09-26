#pragma once

#include "../../common.h"
#include "../ipc.h"
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
        sm, set_sys, apm, apm_ISession, am_appletOE, am_IApplicationProxy, am_ICommonStateGetter, am_IApplicationFunctions, am_ISelfController, am_ILibraryAppletCreator
    };

    /**
     * @brief A map from every service's name as a std::string to the corresponding serviceEnum
     */
    const static std::unordered_map<std::string, Service> ServiceString = {
        {"sm:", Service::sm},
        {"set:sys", Service::set_sys},
        {"apm", Service::apm},
        {"appletOE", Service::am_appletOE},
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
        uint numSessions{}; //<! The amount of active sessions
        const bool asLoop; //<! If the service has a loop or not

        /**
         * @param state The state of the device
         * @param hasLoop If the service has a loop or not
         */
        BaseService(const DeviceState &state, ServiceManager& manager, bool hasLoop, Service serviceType, const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> &vTable) : state(state), manager(manager), hasLoop(hasLoop), serviceType(serviceType), vTable(vTable) {}

        /**
         * @brief This handles all IPC commands with type request to a service
         * @param request The corresponding IpcRequest object
         * @param response The corresponding IpcResponse object
         */
        void HandleRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
            try {
                for(auto& i : vTable)
                    state.logger->Write(Logger::Info, "Service has cmdid [0x{:X}]", i.first);

                vTable.at(request.payload->value)(session, request, response);
            } catch (std::out_of_range&) {
                state.logger->Write(Logger::Warn, "Cannot find function in service with type {0}: 0x{1:X} ({1})", serviceType, u32(request.payload->value));
            }
        };

        /**
         * @brief This is used by some services when they need to run some code at regular intervals
         */
        virtual void Loop() {};
    };
}
