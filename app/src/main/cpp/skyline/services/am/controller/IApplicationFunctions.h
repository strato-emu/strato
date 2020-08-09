// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief This has functions that are used to notify an application about it's state (https://switchbrew.org/wiki/Applet_Manager_services#IApplicationFunctions)
     */
    class IApplicationFunctions : public BaseService {
      private:
        std::shared_ptr<type::KEvent> gpuErrorEvent; //!< The event signalled on GPU errors

      public:
        IApplicationFunctions(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns an Applet Manager IStorage containing the application's launch parameters (https://switchbrew.org/wiki/Applet_Manager_services#PopLaunchParameter)
         */
        void PopLaunchParameter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This creates a save data folder for the requesting application (https://switchbrew.org/wiki/Applet_Manager_services#EnsureSaveData)
         */
        void EnsureSaveData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns the desired language for the application (https://switchbrew.org/wiki/Applet_Manager_services#GetDesiredLanguage)
         */
        void GetDesiredLanguage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns if the application is running or not, always returns true (https://switchbrew.org/wiki/Applet_Manager_services#NotifyRunning)
         */
        void NotifyRunning(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a UUID, however what it refers to is currently unknown (https://switchbrew.org/wiki/Applet_Manager_services#GetPseudoDeviceId)
         */
        void GetPseudoDeviceId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This initializes gameplay recording (https://switchbrew.org/wiki/Applet_Manager_services#InitializeGamePlayRecording)
         */
        void InitializeGamePlayRecording(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This controls the gameplay recording state (https://switchbrew.org/wiki/Applet_Manager_services#SetGamePlayRecordingState)
         */
        void SetGamePlayRecordingState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This obtains a handle to the system GPU error KEvent (https://switchbrew.org/wiki/Applet_Manager_services#GetGpuErrorDetectedSystemEvent)
         */
        void GetGpuErrorDetectedSystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
