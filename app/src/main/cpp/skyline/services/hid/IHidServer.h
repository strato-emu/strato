// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include "IAppletResource.h"

namespace skyline::service::hid {
    /**
     * @brief IHidServer or hid service is used to access input devices (https://switchbrew.org/wiki/HID_services#hid)
     */
    class IHidServer : public BaseService {
      public:
        IHidServer(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns an IAppletResource (https://switchbrew.org/wiki/HID_services#CreateAppletResource)
         */
        Result CreateAppletResource(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This would activate the debug pad (if hid:dbg was used) on a development device
         */
        Result ActivateDebugPad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This activates the touch screen (if it's disabled, it is enabled by default)
         */
        Result ActivateTouchScreen(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This sets the style of controllers supported (https://switchbrew.org/wiki/HID_services#SetSupportedNpadStyleSet)
         */
        Result SetSupportedNpadStyleSet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This gets the style of controllers supported (https://switchbrew.org/wiki/HID_services#GetSupportedNpadStyleSet)
         */
        Result GetSupportedNpadStyleSet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This sets the NpadIds which are supported (https://switchbrew.org/wiki/HID_services#SetSupportedNpadIdType)
         */
        Result SetSupportedNpadIdType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This requests the activation of controllers (https://switchbrew.org/wiki/HID_services#ActivateNpad)
         */
        Result ActivateNpad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This requests the deactivation of controllers (https://switchbrew.org/wiki/HID_services#DeactivateNpad)
         */
        Result DeactivateNpad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This requests an event that's signalled on a specific NpadId changing (https://switchbrew.org/wiki/HID_services#AcquireNpadStyleSetUpdateEventHandle)
         */
        Result AcquireNpadStyleSetUpdateEventHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This requests the LED pattern which represents a particular Player
         */
        Result GetPlayerLedPattern(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This requests the activation of controllers with a specific HID revision (https://switchbrew.org/wiki/HID_services#ActivateNpadWithRevision)
         */
        Result ActivateNpadWithRevision(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the Joy-Con hold mode (https://switchbrew.org/wiki/HID_services#SetNpadJoyHoldType)
         */
        Result SetNpadJoyHoldType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the Joy-Con hold mode (https://switchbrew.org/wiki/HID_services#GetNpadJoyHoldType)
         */
        Result GetNpadJoyHoldType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the Joy-Con assignment mode to Single by default (https://switchbrew.org/wiki/HID_services#SetNpadJoyAssignmentModeSingleByDefault)
         */
        Result SetNpadJoyAssignmentModeSingleByDefault(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the Joy-Con assignment mode to Single (https://switchbrew.org/wiki/HID_services#SetNpadJoyAssignmentModeSingle)
         */
        Result SetNpadJoyAssignmentModeSingle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the Joy-Con assignment mode to Dual (https://switchbrew.org/wiki/HID_services#SetNpadJoyAssignmentModeDual)
         */
        Result SetNpadJoyAssignmentModeDual(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an instance of #IActiveVibrationDeviceList (https://switchbrew.org/wiki/HID_services#CreateActiveVibrationDeviceList)
         */
        Result CreateActiveVibrationDeviceList(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Send vibration values to an NPad (https://switchbrew.org/wiki/HID_services#SendVibrationValues)
         */
        Result SendVibrationValues(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IHidServer, CreateAppletResource),
            SFUNC(0x1, IHidServer, ActivateDebugPad),
            SFUNC(0xB, IHidServer, ActivateTouchScreen),
            SFUNC(0x64, IHidServer, SetSupportedNpadStyleSet),
            SFUNC(0x65, IHidServer, GetSupportedNpadStyleSet),
            SFUNC(0x66, IHidServer, SetSupportedNpadIdType),
            SFUNC(0x67, IHidServer, ActivateNpad),
            SFUNC(0x68, IHidServer, DeactivateNpad),
            SFUNC(0x6A, IHidServer, AcquireNpadStyleSetUpdateEventHandle),
            SFUNC(0x6C, IHidServer, GetPlayerLedPattern),
            SFUNC(0x6D, IHidServer, ActivateNpadWithRevision),
            SFUNC(0x78, IHidServer, SetNpadJoyHoldType),
            SFUNC(0x79, IHidServer, GetNpadJoyHoldType),
            SFUNC(0x7A, IHidServer, SetNpadJoyAssignmentModeSingleByDefault),
            SFUNC(0x7B, IHidServer, SetNpadJoyAssignmentModeSingle),
            SFUNC(0x7C, IHidServer, SetNpadJoyAssignmentModeDual),
            SFUNC(0xCB, IHidServer, CreateActiveVibrationDeviceList),
            SFUNC(0xCE, IHidServer, SendVibrationValues)
        )
    };
}
