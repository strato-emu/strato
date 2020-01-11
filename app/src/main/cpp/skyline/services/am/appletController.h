#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include <kernel/types/KProcess.h>
#include <kernel/types/KEvent.h>
#include <gpu.h>

namespace skyline::service::am {
    /**
     * @brief https://switchbrew.org/wiki/Applet_Manager_services#ICommonStateGetter
     */
    class ICommonStateGetter : public BaseService {
      private:
        /**
         * @brief This enumerates all the possible contents of a #AppletMessage (https://switchbrew.org/wiki/Applet_Manager_services#AppletMessage)
         */
        enum class Message : u32 {
            ExitRequested = 0x4, //!< The applet has been requested to exit
            FocusStateChange = 0xF, //!< There was a change in the focus state of the applet
            ExecutionResumed = 0x10, //!< The execution of the applet has resumed
            OperationModeChange = 0x1E, //!< There was a change in the operation mode
            PerformanceModeChange = 0x1F, //!< There was a change in the performance mode
            RequestToDisplay = 0x33, //!< This indicates that ApproveToDisplay should be used
            CaptureButtonShortPressed = 0x5A, //!< The Capture button was short pressed
            ScreenshotTaken = 0x5C //!< A screenshot was taken
        };

        std::shared_ptr<type::KEvent> messageEvent; //!< The event signalled when there is a message available
        std::queue<Message> messageQueue;

        enum class FocusState : u8 {
            InFocus = 1, //!< The application is in foreground
            OutOfFocus = 2 //!< The application is in the background
        } focusState{FocusState::InFocus};

        enum class OperationMode : u8 {
            Handheld = 0, //!< The device is in handheld mode
            Docked = 1 //!< The device is in docked mode
        } operationMode;

        /**
         * @brief This queues a message for the application to read via ReceiveMessage
         * @param message The message to queue
         */
        void QueueMessage(Message message);

      public:
        ICommonStateGetter(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns the handle to a KEvent object that is signalled whenever RecieveMessage has a message (https://switchbrew.org/wiki/Applet_Manager_services#GetEventHandle)
         */
        void GetEventHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns an #AppletMessage or 0x680 to indicate the lack of a message (https://switchbrew.org/wiki/Applet_Manager_services#ReceiveMessage)
         */
        void ReceiveMessage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

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

        /**
         * @brief This returns the current display width and height in two u32s (https://switchbrew.org/wiki/Applet_Manager_services#GetDefaultDisplayResolution)
         */
        void GetDefaultDisplayResolution(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief This has functions relating to an application's own current status (https://switchbrew.org/wiki/Applet_Manager_services#ISelfController)
     */
    class ISelfController : public BaseService {
      public:
        ISelfController(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This function takes a u8 bool flag and no output (Stubbed) (https://switchbrew.org/wiki/Applet_Manager_services#SetOperationModeChangedNotification)
         */
        void SetOperationModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function takes a u8 bool flag and no output (Stubbed) (https://switchbrew.org/wiki/Applet_Manager_services#SetPerformanceModeChangedNotification)
         */
        void SetPerformanceModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function takes 3 unknown u8 values and has no output (Stubbed) (https://switchbrew.org/wiki/Applet_Manager_services#GetCurrentFocusState)
         */
        void SetFocusHandlingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function takes a u8 bool flag and has no output (Stubbed) (https://switchbrew.org/wiki/Applet_Manager_services#SetOutOfFocusSuspendingEnabled)
         */
        void SetOutOfFocusSuspendingEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function returns an output u64 LayerId (https://switchbrew.org/wiki/Applet_Manager_services#CreateManagedDisplayLayer)
         */
        void CreateManagedDisplayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief This has functions used to retrieve the status of the application's window (https://switchbrew.org/wiki/Applet_Manager_services#IWindowController)
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
     * @brief This has functions relating to volume control (https://switchbrew.org/wiki/Applet_Manager_services#IAudioController)
     */
    class IAudioController : public BaseService {
      public:
        IAudioController(const DeviceState &state, ServiceManager &manager);
    };

    /**
     * @brief This has functions used to capture the contents of a display (https://switchbrew.org/wiki/Applet_Manager_services#IDisplayController)
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
     * @brief This has functions that are used to notify an application about it's state (https://switchbrew.org/wiki/Applet_Manager_services#IApplicationFunctions)
     */
    class IApplicationFunctions : public BaseService {
      public:
        IApplicationFunctions(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns if the application is running or not, always returns true (https://switchbrew.org/wiki/Applet_Manager_services#NotifyRunning)
         */
        void NotifyRunning(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief This has functions that are used for debugging purposes (https://switchbrew.org/wiki/Applet_Manager_services#IDebugFunctions)
     */
    class IDebugFunctions : public BaseService {
      public:
        IDebugFunctions(const DeviceState &state, ServiceManager &manager);
    };

    /**
     * @brief This contains common various functions (https://switchbrew.org/wiki/Applet_Manager_services#IAppletCommonFunctions)
     */
    class IAppletCommonFunctions : public BaseService {
      public:
        IAppletCommonFunctions(const DeviceState &state, ServiceManager &manager);
    };
}
