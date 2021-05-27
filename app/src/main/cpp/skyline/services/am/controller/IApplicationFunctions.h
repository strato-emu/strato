// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    namespace result {
        constexpr Result InvalidParameters(128, 506);
    }

    /**
     * @brief This is used to notify an application about its own state
     * @url https://switchbrew.org/wiki/Applet_Manager_services#IApplicationFunctions
     */
    class IApplicationFunctions : public BaseService {
      private:
        std::shared_ptr<type::KEvent> gpuErrorEvent; //!< The event signalled on GPU errors

      public:
        IApplicationFunctions(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns an Applet Manager IStorage containing the application's launch parameters
         * @url https://switchbrew.org/wiki/Applet_Manager_services#PopLaunchParameter
         */
        Result PopLaunchParameter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Creates a save data folder for the requesting application
         * @url https://switchbrew.org/wiki/Applet_Manager_services#EnsureSaveData
         */
        Result EnsureSaveData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the desired language for the application
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetDesiredLanguage
         */
        Result GetDesiredLanguage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns if the application is running or not, always returns true
         * @url https://switchbrew.org/wiki/Applet_Manager_services#NotifyRunning
         */
        Result NotifyRunning(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a V5 UUID generated from a seed in control.nacp and a device specific seed
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetPseudoDeviceId
         */
        Result GetPseudoDeviceId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Initializes gameplay recording
         * @url https://switchbrew.org/wiki/Applet_Manager_services#InitializeGamePlayRecording
         */
        Result InitializeGamePlayRecording(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the gameplay recording state
         * @url https://switchbrew.org/wiki/Applet_Manager_services#SetGamePlayRecordingState
         */
        Result SetGamePlayRecordingState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Uses the given transfer memory to setup memory for the screenshot copyright image
         * @url https://switchbrew.org/wiki/Applet_Manager_services#InitializeApplicationCopyrightFrameBuffer
         */
        Result InitializeApplicationCopyrightFrameBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the copyright image for screenshots using the buffer from InitializeApplicationCopyrightFrameBuffer
         * @url https://switchbrew.org/wiki/Applet_Manager_services#SetApplicationCopyrightImage
         */
        Result SetApplicationCopyrightImage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Controls the visibility of the screenshot copyright image
         * @url https://switchbrew.org/wiki/Applet_Manager_services#SetApplicationCopyrightVisibility
         */
        Result SetApplicationCopyrightVisibility(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Obtains a handle to the system GPU error KEvent
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetGpuErrorDetectedSystemEvent
         */
        Result GetGpuErrorDetectedSystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x1, IApplicationFunctions, PopLaunchParameter),
            SFUNC(0x14, IApplicationFunctions, EnsureSaveData),
            SFUNC(0x15, IApplicationFunctions, GetDesiredLanguage),
            SFUNC(0x28, IApplicationFunctions, NotifyRunning),
            SFUNC(0x32, IApplicationFunctions, GetPseudoDeviceId),
            SFUNC(0x42, IApplicationFunctions, InitializeGamePlayRecording),
            SFUNC(0x43, IApplicationFunctions, SetGamePlayRecordingState),
            SFUNC(0x64, IApplicationFunctions, InitializeApplicationCopyrightFrameBuffer),
            SFUNC(0x65, IApplicationFunctions, SetApplicationCopyrightImage),
            SFUNC(0x66, IApplicationFunctions, SetApplicationCopyrightVisibility),
            SFUNC(0x82, IApplicationFunctions, GetGpuErrorDetectedSystemEvent)
        )

    };
}
