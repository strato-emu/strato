#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief appletOE is used to open an application proxy (https://switchbrew.org/wiki/Applet_Manager_services#appletOE)
     */
    class appletOE : public BaseService {
      public:
        appletOE(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns #IApplicationProxy (https://switchbrew.org/wiki/Applet_Manager_services#OpenApplicationProxy)
         */
        void OpenApplicationProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief appletAE is used to open proxies (https://switchbrew.org/wiki/Applet_Manager_services#appletAE)
     */
    class appletAE : public BaseService {
      public:
        appletAE(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns #ILibraryAppletProxy (https://switchbrew.org/wiki/Applet_Manager_services#OpenLibraryAppletProxy)
         */
        void OpenLibraryAppletProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IApplicationProxy (https://switchbrew.org/wiki/Applet_Manager_services#OpenApplicationProxy)
         */
        void OpenApplicationProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IOverlayAppletProxy (https://switchbrew.org/wiki/Applet_Manager_services#OpenOverlayAppletProxy)
         */
        void OpenOverlayAppletProxy(type::KSession& session, ipc::IpcRequest& request, ipc::IpcResponse& response);

        /**
         * @brief This returns #ISystemAppletProxy (https://switchbrew.org/wiki/Applet_Manager_services#OpenSystemAppletProxy)
         */
        void OpenSystemAppletProxy(type::KSession& session, ipc::IpcRequest& request, ipc::IpcResponse& response);
    };

    /**
     * @brief ILibraryAppletProxy returns handles to various services (https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletProxy)
     */
    class BaseProxy : public BaseService {
      public:
        BaseProxy(const DeviceState &state, ServiceManager &manager, Service serviceType, const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> &vTable);

        /**
         * @brief This returns #ICommonStateGetter (https://switchbrew.org/wiki/Applet_Manager_services#ICommonStateGetter)
         */
        void GetCommonStateGetter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #ISelfController (https://switchbrew.org/wiki/Applet_Manager_services#ISelfController)
         */
        void GetSelfController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IWindowController (https://switchbrew.org/wiki/Applet_Manager_services#IWindowController)
         */
        void GetWindowController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IAudioController (https://switchbrew.org/wiki/Applet_Manager_services#IAudioController)
         */
        void GetAudioController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IDisplayController (https://switchbrew.org/wiki/Applet_Manager_services#IDisplayController)
         */
        void GetDisplayController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #ILibraryAppletCreator (https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletCreator)
         */
        void GetLibraryAppletCreator(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IDebugFunctions (https://switchbrew.org/wiki/Applet_Manager_services#IDebugFunctions)
         */
        void GetDebugFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IAppletCommonFunctions (https://switchbrew.org/wiki/Applet_Manager_services#IAppletCommonFunctions)
         */
        void GetAppletCommonFunctions(type::KSession& session, ipc::IpcRequest& request, ipc::IpcResponse& response);
    };

    /**
     * @brief IApplicationProxy returns handles to various services (https://switchbrew.org/wiki/Applet_Manager_services#IApplicationProxy)
     */
    class IApplicationProxy : public BaseProxy {
      public:
        IApplicationProxy(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns #IApplicationFunctions (https://switchbrew.org/wiki/Applet_Manager_services#IApplicationFunctions)
         */
        void GetApplicationFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief IOverlayAppletProxy returns handles to various services (https://switchbrew.org/wiki/Applet_Manager_services#IOverlayAppletProxy)
     */
    class IOverlayAppletProxy : public BaseProxy {
      public:
        IOverlayAppletProxy(const DeviceState& state, ServiceManager& manager);
    };

    /**
     * @brief ISystemAppletProxy returns handles to various services (https://switchbrew.org/wiki/Applet_Manager_services#ISystemAppletProxy)
     */
    class ISystemAppletProxy : public BaseProxy {
      public:
        ISystemAppletProxy(const DeviceState& state, ServiceManager& manager);
    };

    /**
     * @brief ILibraryAppletProxy returns handles to various services (https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletProxy)
     */
    class ILibraryAppletProxy : public BaseProxy {
      public:
        ILibraryAppletProxy(const DeviceState &state, ServiceManager &manager);
    };
}
