#pragma once

#include "../base_service.h"
#include "../serviceman.h"

namespace skyline::kernel::service::am {
    /**
     * @brief appletOE is used to open an application proxy (https://switchbrew.org/wiki/Applet_Manager_services#appletOE)
     */
    class appletOE : public BaseService {
      public:
        appletOE(const DeviceState &state, ServiceManager& manager);

        /**
         * @brief This returns IApplicationProxy (https://switchbrew.org/wiki/Applet_Manager_services#OpenApplicationProxy)
         */
        void OpenApplicationProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief IApplicationProxy returns handles to various services (https://switchbrew.org/wiki/Applet_Manager_services#IApplicationProxy)
     */
    class IApplicationProxy : public BaseService {
      public:
        IApplicationProxy(const DeviceState &state, ServiceManager& manager);

        /**
         * @brief This returns #ICommonStateGetter (https://switchbrew.org/wiki/Applet_Manager_services#ICommonStateGetter)
         */
        void GetCommonStateGetter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #ISelfController (https://switchbrew.org/wiki/Applet_Manager_services#ISelfController)
         */
        void GetSelfController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #ILibraryAppletCreator (https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletCreator)
         */
        void GetLibraryAppletCreator(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IApplicationFunctions (https://switchbrew.org/wiki/Applet_Manager_services#IApplicationFunctions)
         */
        void GetApplicationFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    class ICommonStateGetter : public BaseService {
      public:
        ICommonStateGetter(const DeviceState &state, ServiceManager &manager);
    };

    class ISelfController : public BaseService {
      public:
        ISelfController(const DeviceState &state, ServiceManager &manager);
    };

    class ILibraryAppletCreator : public BaseService {
      public:
        ILibraryAppletCreator(const DeviceState &state, ServiceManager &manager);
    };

    class IApplicationFunctions : public BaseService {
      public:
        IApplicationFunctions(const DeviceState &state, ServiceManager &manager);
    };
}
