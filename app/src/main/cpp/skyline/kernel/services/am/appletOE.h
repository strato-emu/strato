#pragma once

#include <kernel/services/base_service.h>
#include <kernel/services/serviceman.h>
#include <kernel/types/KProcess.h>
#include <kernel/types/KEvent.h>

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
         * @brief This returns #IApplicationFunctions (https://switchbrew.org/wiki/Applet_Manager_services#IApplicationFunctions)
         */
        void GetApplicationFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IDebugFunctions (https://switchbrew.org/wiki/Applet_Manager_services#IDebugFunctions)
         */
        void IDebugFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief https://switchbrew.org/wiki/Applet_Manager_services#ICommonStateGetter
     */
    class ICommonStateGetter : public BaseService {
      private:
        std::shared_ptr<type::KEvent> messageEvent{};

        enum class ApplicationStatus : u8 {
            InFocus = 1, OutOfFocus = 2
        };

        enum class OperationMode : u8 {
            Handheld = 0, Docked = 1
        } operationMode;
      public:
        ICommonStateGetter(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns the handle to a KEvent object that is signalled whenever RecieveMessage has a message (https://switchbrew.org/wiki/Applet_Manager_services#GetEventHandle)
         */
        void GetEventHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns if an application is in focus or not. It always returns in focus on the emulator (https://switchbrew.org/wiki/Applet_Manager_services#GetCurrentFocusState)
         */
        void GetCurrentFocusState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns the current OperationMode (https://switchbrew.org/wiki/Applet_Manager_services#GetOperationMode)
         */
        void GetOperationMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns the current PerformanceMode (Same as operationMode but u32) (https://switchbrew.org/wiki/Applet_Manager_services#GetPerformanceMode)
         */
        void GetPerformanceMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief https://switchbrew.org/wiki/Applet_Manager_services#ISelfController
     */
    class ISelfController : public BaseService {
      public:
        ISelfController(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This function inputs a u8 bool flag and no output (Stubbed) (https://switchbrew.org/wiki/Applet_Manager_services#SetOperationModeChangedNotification)
         */
        void SetOperationModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function inputs a u8 bool flag and no output (Stubbed) (https://switchbrew.org/wiki/Applet_Manager_services#SetPerformanceModeChangedNotification)
         */
        void SetPerformanceModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function inputs 3 unknown u8 values and has no output (Stubbed) (https://switchbrew.org/wiki/Applet_Manager_services#GetCurrentFocusState)
         */
        void SetFocusHandlingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief https://switchbrew.org/wiki/Applet_Manager_services#IWindowController
     */
    class IWindowController : public BaseService {
      public:
        IWindowController(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns the PID of the current application (https://switchbrew.org/wiki/Applet_Manager_services#GetAppletResourceUserId)
         */
        void GetAppletResourceUserId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function has mo inputs or outputs (Stubbed) (https://switchbrew.org/wiki/Applet_Manager_services#AcquireForegroundRights)
         */
        void AcquireForegroundRights(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief https://switchbrew.org/wiki/Applet_Manager_services#IAudioController
     */
    class IAudioController : public BaseService {
      public:
        IAudioController(const DeviceState &state, ServiceManager &manager);
    };

    /**
     * @brief https://switchbrew.org/wiki/Applet_Manager_services#IDisplayController
     */
    class IDisplayController : public BaseService {
      public:
        IDisplayController(const DeviceState &state, ServiceManager &manager);
    };

    /**
     * @brief https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletCreator
     */
    class ILibraryAppletCreator : public BaseService {
      public:
        ILibraryAppletCreator(const DeviceState &state, ServiceManager &manager);
    };

    /**
     * @brief https://switchbrew.org/wiki/Applet_Manager_services#IApplicationFunctions
     */
    class IApplicationFunctions : public BaseService {
      public:
        IApplicationFunctions(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This just returns 1 regardless of input (https://switchbrew.org/wiki/Applet_Manager_services#NotifyRunning)
         */
        void NotifyRunning(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief https://switchbrew.org/wiki/Applet_Manager_services#IDebugFunctions
     */
    class IDebugFunctions : public BaseService {
      public:
        IDebugFunctions(const DeviceState &state, ServiceManager &manager);
    };
}
