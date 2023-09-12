// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::pctl {
    namespace result {
        constexpr Result StereoVisionDenied(142, 104);
        constexpr Result PermissionDenied(142, 133);
        constexpr Result StereoVisionRestrictionConfigurableDisabled(142, 181);
    }

    /**
     * @brief IParentalControlService is used to access parental control configuration
     * @url https://switchbrew.org/wiki/Parental_Control_services#IParentalControlService
     */
    class IParentalControlService : public BaseService {
      private:
        bool featuresRestriction{false};
        bool stereoVisionRestrictionConfigurable{true};
        bool stereoVisionRestriction{false};

        Result IsStereoVisionPermittedImpl();

      public:
        IParentalControlService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Initialises the parental control service instance
         */
        Result Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result CheckFreeCommunicationPermission(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result ConfirmStereoVisionPermission(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result EndFreeCommunication(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result IsFreeCommunicationAvailable(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result ConfirmStereoVisionRestrictionConfigurable(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetStereoVisionRestriction(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetStereoVisionRestriction(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result ResetConfirmedStereoVisionPermission(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result IsStereoVisionPermitted(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

      SERVICE_DECL(
            SFUNC(0x1, IParentalControlService, Initialize),
            SFUNC(0x3E9, IParentalControlService, CheckFreeCommunicationPermission),
            SFUNC(0x3F5, IParentalControlService, ConfirmStereoVisionPermission),
            SFUNC(0x3F9, IParentalControlService, EndFreeCommunication),
            SFUNC(0x3FA, IParentalControlService, IsFreeCommunicationAvailable),
            SFUNC(0x425, IParentalControlService, ConfirmStereoVisionRestrictionConfigurable),
            SFUNC(0x426, IParentalControlService, GetStereoVisionRestriction),
            SFUNC(0x427, IParentalControlService, SetStereoVisionRestriction),
            SFUNC(0x428, IParentalControlService, ResetConfirmedStereoVisionPermission),
            SFUNC(0x429, IParentalControlService, IsStereoVisionPermitted)
        )
    };
}
