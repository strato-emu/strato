// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/base_service.h>

namespace skyline::service::nfp {
    /**
     * @brief IUser is used by applications to access NFP (Nintendo Figurine Protocol) devices
     * @url https://switchbrew.org/wiki/NFC_services#IUser_3
     */
    class IUser : public BaseService {
      private:
        std::shared_ptr<type::KEvent> attachAvailabilityChangeEvent; //!< Signalled on NFC device availability changes

        enum class State : u32 {
            NotInitialized = 0,
            Initialized = 1
        } nfpState{State::NotInitialized};

      public:
        IUser(const DeviceState &state, ServiceManager &manager);

        Result Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#ListDevices
         */
        Result ListDevices(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#GetState_2
         */
        Result GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @url https://switchbrew.org/wiki/NFC_services#AttachAvailabilityChangeEvent
         */
        Result AttachAvailabilityChangeEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IUser, Initialize),
            SFUNC(0x2, IUser, ListDevices),
            SFUNC(0x13, IUser, GetState),
            SFUNC(0x17, IUser, AttachAvailabilityChangeEvent)
        )
    };
}
