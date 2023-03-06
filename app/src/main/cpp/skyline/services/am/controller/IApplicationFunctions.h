// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    namespace result {
        constexpr Result NotAvailable(128, 2);
        constexpr Result InvalidInput(128, 500);
        constexpr Result InvalidParameters(128, 506);
    }

    /**
     * @brief This is used to notify an application about its own state
     * @url https://switchbrew.org/wiki/Applet_Manager_services#IApplicationFunctions
     */
    class IApplicationFunctions : public BaseService {
      private:
        std::shared_ptr<type::KEvent> gpuErrorEvent; //!< The event signalled on GPU errors
        std::shared_ptr<type::KEvent> friendInvitationStorageChannelEvent; //!< The event signalled on friend invitations
        std::shared_ptr<type::KEvent> notificationStorageChannelEvent;
        i32 previousProgramIndex{-1}; //!< There was no previous title

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
         * @brief Sets a termination result for the application
         * @url https://switchbrew.org/wiki/Applet_Manager_services#SetTerminateResult
         */
        Result SetTerminateResult(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the desired language for the application
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetDesiredLanguage
         */
        Result GetDesiredLanguage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetDisplayVersion
         */
        Result GetDisplayVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetSaveDataSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

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
         * @url https://switchbrew.org/wiki/Applet_Manager_services#EnableApplicationCrashReport
         */
        Result EnableApplicationCrashReport(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

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
         * @url https://switchbrew.org/wiki/Applet_Manager_services#QueryApplicationPlayStatistics
         */
        Result QueryApplicationPlayStatistics(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Applet_Manager_services#QueryApplicationPlayStatisticsByUid
         */
        Result QueryApplicationPlayStatisticsByUid(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Gets the ProgramIndex of the Application which launched this title
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetPreviousProgramIndex
         */
        Result GetPreviousProgramIndex(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Obtains a handle to the system GPU error KEvent
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetGpuErrorDetectedSystemEvent
         */
        Result GetGpuErrorDetectedSystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetFriendInvitationStorageChannelEvent
         */
        Result GetFriendInvitationStorageChannelEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result TryPopFromFriendInvitationStorageChannel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Obtains a handle to the Notification StorageChannel KEvent
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetNotificationStorageChannelEvent
         */
        Result GetNotificationStorageChannelEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x1, IApplicationFunctions, PopLaunchParameter),
            SFUNC(0x14, IApplicationFunctions, EnsureSaveData),
            SFUNC(0x15, IApplicationFunctions, GetDesiredLanguage),
            SFUNC(0x16, IApplicationFunctions, SetTerminateResult),
            SFUNC(0x17, IApplicationFunctions, GetDisplayVersion),
            SFUNC(0x1A, IApplicationFunctions, GetSaveDataSize),
            SFUNC(0x28, IApplicationFunctions, NotifyRunning),
            SFUNC(0x32, IApplicationFunctions, GetPseudoDeviceId),
            SFUNC(0x42, IApplicationFunctions, InitializeGamePlayRecording),
            SFUNC(0x43, IApplicationFunctions, SetGamePlayRecordingState),
            SFUNC(0x5A, IApplicationFunctions, EnableApplicationCrashReport),
            SFUNC(0x64, IApplicationFunctions, InitializeApplicationCopyrightFrameBuffer),
            SFUNC(0x65, IApplicationFunctions, SetApplicationCopyrightImage),
            SFUNC(0x66, IApplicationFunctions, SetApplicationCopyrightVisibility),
            SFUNC(0x6E, IApplicationFunctions, QueryApplicationPlayStatistics),
            SFUNC(0x6F, IApplicationFunctions, QueryApplicationPlayStatisticsByUid),
            SFUNC(0x7B, IApplicationFunctions, GetPreviousProgramIndex),
            SFUNC(0x82, IApplicationFunctions, GetGpuErrorDetectedSystemEvent),
            SFUNC(0x8C, IApplicationFunctions, GetFriendInvitationStorageChannelEvent),
            SFUNC(0x8D, IApplicationFunctions, TryPopFromFriendInvitationStorageChannel),
            SFUNC(0x96, IApplicationFunctions, GetNotificationStorageChannelEvent)
        )

    };
}
