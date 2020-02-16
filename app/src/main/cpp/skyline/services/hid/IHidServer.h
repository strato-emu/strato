#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include "IAppletResource.h"

namespace skyline::service::hid {
    /**
     * @brief IHidServer or hid service is used to access input devices (https://switchbrew.org/wiki/HID_services#hid)
     */
    class IHidServer : public BaseService {
      private:
        /**
         * @brief This holds the controller styles supported by an application
         */
        struct StyleSet {
            bool proController : 1; //!< The Pro Controller
            bool joyconHandheld : 1; //!< Joy-Cons in handheld mode
            bool joyconDual : 1; //!< Joy-Cons in a pair
            bool joyconLeft : 1; //!< Left Joy-Con only
            bool joyconRight : 1; //!< Right Joy-Con only
            bool gamecube : 1; //!< GameCube controller
            bool pokeball : 1;  //!< Poké Ball Plus controller
            bool nes : 1; //!< NES controller
            bool nesHandheld : 1; //!< NES controller in handheld mode
            bool snes : 1; //!< SNES controller
            u32 _pad0_ : 22;
        };
        static_assert(sizeof(StyleSet) == 4);

        /**
         * @brief This holds a Controller's ID (https://switchbrew.org/wiki/HID_services#NpadIdType)
         */
        enum class NpadId : u32 {
            Player1 = 0x0, //!< 1st Player
            Player2 = 0x1, //!< 2nd Player
            Player3 = 0x2, //!< 3rd Player
            Player4 = 0x3, //!< 4th Player
            Player5 = 0x4, //!< 5th Player
            Player6 = 0x5, //!< 6th Player
            Player7 = 0x6, //!< 7th Player
            Player8 = 0x7, //!< 8th Player
            Unknown = 0x10, //!< Unknown
            Handheld = 0x20 //!< Handheld mode
        };

        /**
         * @brief This holds a Controller's Assignment mode
         */
        enum class JoyConAssignment {
            Dual, //!< Dual Joy-Cons
            Single, //!< Single Joy-Con
            Unset //!< Not set
        };

        /**
         * @brief This holds which Joy-Con to use Single mode (Not if SetNpadJoyAssignmentModeSingleByDefault is used)
         */
        enum class JoyConSide : i64 {
            Left, //!< Left Joy-Con
            Right, //!< Right Joy-Con
            Unset //!< Not set
        };

        /**
         * @brief This denotes the orientation of the Joy-Con(s)
         */
        enum class JoyConOrientation : u64 {
            Vertical, //!< The Joy-Con is held vertically
            Horizontal, //!< The Joy-Con is held horizontally
            Unset //!< Not set
        };

        // TODO: Replace JoyConDevice with base NpadDevice class

        /**
         * @brief This holds the state of a single Npad device
         */
        struct JoyConDevice {
            NpadId id; //!< The ID of this device
            JoyConAssignment assignment{JoyConAssignment::Unset}; //!< The assignment mode of this device
            JoyConSide side{JoyConSide::Unset}; //!< The type of the device

            JoyConDevice() : id(NpadId::Unknown) {}

            JoyConDevice(const NpadId &id) : id(id) {}
        };

        std::shared_ptr <IAppletResource> resource{}; //!< A shared pointer to the applet resource
        std::optional <StyleSet> styleSet; //!< The controller styles supported by the application
        std::unordered_map <NpadId, JoyConDevice> deviceMap; //!< Mapping from a controller's ID to it's corresponding JoyConDevice
        JoyConOrientation orientation{JoyConOrientation::Unset}; //!< The Orientation of the Joy-Con(s)

      public:
        IHidServer(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns an IAppletResource (https://switchbrew.org/wiki/HID_services#CreateAppletResource)
         */
        void CreateAppletResource(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This sets the style of controllers supported (https://switchbrew.org/wiki/HID_services#SetSupportedNpadStyleSet)
         */
        void SetSupportedNpadStyleSet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This sets the NpadIds which are supported (https://switchbrew.org/wiki/HID_services#SetSupportedNpadIdType)
         */
        void SetSupportedNpadIdType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This requests the activation of a controller. This is stubbed as we don't have to activate anything.
         */
        void ActivateNpad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the Joy-Con hold mode (https://switchbrew.org/wiki/HID_services#SetNpadJoyHoldType)
         */
        void SetNpadJoyHoldType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the Joy-Con assignment mode to Single by default (https://switchbrew.org/wiki/HID_services#SetNpadJoyAssignmentModeSingleByDefault)
         */
        void SetNpadJoyAssignmentModeSingleByDefault(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the Joy-Con assignment mode to Single (https://switchbrew.org/wiki/HID_services#SetNpadJoyAssignmentModeSingle)
         */
        void SetNpadJoyAssignmentModeSingle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the Joy-Con assignment mode to Dual (https://switchbrew.org/wiki/HID_services#SetNpadJoyAssignmentModeDual)
         */
        void SetNpadJoyAssignmentModeDual(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
