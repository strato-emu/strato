// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <queue>
#include <kernel/types/KEvent.h>
#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    namespace result {
        constexpr Result NoMessages(128, 3);
    }

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
        Result GetEventHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns an #AppletMessage or 0x680 to indicate the lack of a message (https://switchbrew.org/wiki/Applet_Manager_services#ReceiveMessage)
         */
        Result ReceiveMessage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns if an application is in focus or not. It always returns in focus on the emulator (https://switchbrew.org/wiki/Applet_Manager_services#GetCurrentFocusState)
         */
        Result GetCurrentFocusState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns the current OperationMode (https://switchbrew.org/wiki/Applet_Manager_services#GetOperationMode)
         */
        Result GetOperationMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns the current PerformanceMode (Same as operationMode but u32) (https://switchbrew.org/wiki/Applet_Manager_services#GetPerformanceMode)
         */
        Result GetPerformanceMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns the current display width and height in two u32s (https://switchbrew.org/wiki/Applet_Manager_services#GetDefaultDisplayResolution)
         */
        Result GetDefaultDisplayResolution(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
