// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <deque>
#include <kernel/types/KEvent.h>
#include <services/serviceman.h>
#include <common/macros.h>

namespace skyline::service::am {
    namespace result {
        constexpr Result NoMessages(128, 3);
        constexpr Result InvalidParameters(128, 506);
    }

    /**
     * @brief https://switchbrew.org/wiki/Applet_Manager_services#ICommonStateGetter
     */
    class ICommonStateGetter : public BaseService {
      private:
        /**
         * @brief All the possible contents of a #AppletMessage
         * @url https://switchbrew.org/wiki/Applet_Manager_services#AppletMessage
         */
        enum class Message : u32 {
            ExitRequested = 0x4,              //!< The applet has been requested to exit
            FocusStateChange = 0xF,           //!< There was a change in the focus state of the applet
            ExecutionResumed = 0x10,          //!< The execution of the applet has resumed
            OperationModeChange = 0x1E,       //!< There was a change in the operation mode
            PerformanceModeChange = 0x1F,     //!< There was a change in the performance mode
            RequestToDisplay = 0x33,          //!< Indicates that ApproveToDisplay should be used
            CaptureButtonShortPressed = 0x5A, //!< The Capture button was short pressed
            ScreenshotTaken = 0x5C,           //!< A screenshot was taken
        };

        std::shared_ptr<type::KEvent> messageEvent; //!< The event signalled when there is a message available
        std::deque<Message> messageQueue; //!< A queue of all the messages that the program is yet to consume

        enum class FocusState : u8 {
            InFocus = 1,    //!< The application is in foreground
            OutOfFocus = 2, //!< The application is in the background
        } focusState{FocusState::InFocus};

        enum class OperationMode : u8 {
            Handheld = 0, //!< The device is in handheld mode
            Docked = 1,   //!< The device is in docked mode
        } operationMode;

        enum class CpuBoostMode : u32 {
            Normal = 0,     //!< The device runs at stock CPU and CPU clocks
            FastLoad = 1,   //!< The device runs at boosted CPU clocks and minimum GPU clocks
            PowerSaving = 2 //!< The device runs at stock CPU clocks and minimum GPU clocks
        } cpuBoostMode;

        ENUM_STRING(CpuBoostMode, {
            ENUM_CASE_PAIR(Normal, "Normal");
            ENUM_CASE_PAIR(FastLoad, "Fast Load");
            ENUM_CASE_PAIR(PowerSaving, "Power Saving");
        })

        std::shared_ptr<type::KEvent> defaultDisplayResolutionChangeEvent; //!< Signalled when the default display resolution changes,

        /**
         * @brief Queues a message for the application to read via ReceiveMessage
         * @param message The message to queue
         */
        void QueueMessage(Message message);

      public:
        ICommonStateGetter(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns the handle to a KEvent object that is signalled whenever RecieveMessage has a message
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetEventHandle
         */
        Result GetEventHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an #AppletMessage or 0x680 to indicate the lack of a message
         * @url https://switchbrew.org/wiki/Applet_Manager_services#ReceiveMessage
         */
        Result ReceiveMessage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns if an application is in focus or not. It always returns in focus on the emulator
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetCurrentFocusState
         */
        Result GetCurrentFocusState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the current OperationMode
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetOperationMode
         */
        Result GetOperationMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the current PerformanceMode (Same as operationMode but u32)
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetPerformanceMode
         */
        Result GetPerformanceMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the state of VR mode
         * @url https://switchbrew.org/wiki/Applet_Manager_services#IsVrModeEnabled
         */
        Result IsVrModeEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the current display width and height in two u32s
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetDefaultDisplayResolution
         */
        Result GetDefaultDisplayResolution(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetDefaultDisplayResolutionChangeEvent
         */
        Result GetDefaultDisplayResolutionChangeEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the CPU boost mode to the supplied value
         * @url https://switchbrew.org/wiki/Applet_Manager_services#SetCpuBoostMode
         */
        Result SetCpuBoostMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Applet_Manager_services#SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled
         */
        Result SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, ICommonStateGetter, GetEventHandle),
            SFUNC(0x1, ICommonStateGetter, ReceiveMessage),
            SFUNC(0x5, ICommonStateGetter, GetOperationMode),
            SFUNC(0x6, ICommonStateGetter, GetPerformanceMode),
            SFUNC(0x9, ICommonStateGetter, GetCurrentFocusState),
            SFUNC(0x32, ICommonStateGetter, IsVrModeEnabled),
            SFUNC(0x3C, ICommonStateGetter, GetDefaultDisplayResolution),
            SFUNC(0x3D, ICommonStateGetter, GetDefaultDisplayResolutionChangeEvent),
            SFUNC(0x42, ICommonStateGetter, SetCpuBoostMode),
            SFUNC(0x384, ICommonStateGetter, SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled)
        )
    };
}
