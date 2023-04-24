// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::ldn {
    namespace result {
        constexpr Result AirplaneModeEnabled{203, 23};
        constexpr Result InvalidInput{203, 96};
    }

    constexpr size_t SsidLengthMax = 32;
    constexpr size_t UserNameBytesMax = 32;
    constexpr i32 NodeCountMax = 8;
    constexpr size_t AdvertiseDataSizeMax = 384;
    constexpr size_t PassphraseLengthMax = 64;

    enum class State : u32 {
        None,
        Initialized,
        AccessPointOpened,
        AccessPointCreated,
        StationOpened,
        StationConnected,
        Error,
    };

    enum class DisconnectReason : i16 {
        Unknown = -1,
        None,
        User,
        System,
        DestroyedByUser,
        DestroyedBySystemRequest,
        Admin,
        SignalLost,
    };

    enum class WifiChannel : i16 {
        Default = 0,
        Wifi24_1 = 1,
        Wifi24_6 = 6,
        Wifi24_11 = 11,
        Wifi50_36 = 36,
        Wifi50_40 = 40,
        Wifi50_44 = 44,
        Wifi50_48 = 48,
    };

    enum class LinkLevel : i8 {
        Bad,
        Low,
        Good,
        Excellent,
    };

    enum class PackedNetworkType : u8 {
        None,
        General,
        Ldn,
        All,
    };

    enum class SecurityMode : u16 {
        All,
        Retail,
        Debug,
    };

    enum class AcceptPolicy : u8 {
        AcceptAll,
        RejectAll,
        BlackList,
        WhiteList,
    };

    enum class NodeStateChange : u8 {
        None,
        Connect,
        Disconnect,
        DisconnectAndConnect,
    };

    struct IntentId {
        u64 localCommunicationId;
        u8 _pad0_[0x2];
        u16 sceneId;
        u8 _pad1_[0x4];
    };
    static_assert(sizeof(IntentId) == 0x10);

    struct SessionId {
        u64 high;
        u64 low;
    };
    static_assert(sizeof(SessionId) == 0x10);

    struct NetworkId {
        IntentId intentId;
        SessionId sessionId;
    };
    static_assert(sizeof(NetworkId) == 0x20);

    struct MacAddress {
        std::array<u8, 6> raw{};
    };
    static_assert(sizeof(MacAddress) == 0x6);

    struct Ssid {
        u8 length{};
        std::array<char, SsidLengthMax + 1> raw{};
    };
    static_assert(sizeof(Ssid) == 0x22);

    struct CommonNetworkInfo {
        MacAddress bssid;
        Ssid ssid;
        WifiChannel channel;
        LinkLevel linkLevel;
        PackedNetworkType networkType;
        u8 _pad0_[0x4];
    };
    static_assert(sizeof(CommonNetworkInfo) == 0x30);

    struct NodeInfo {
        std::array<u8, 4> ipv4Address;
        MacAddress macAddress;
        i8 nodeId;
        u8 isConnected;
        std::array<u8, UserNameBytesMax + 1> username;
        u8 _pad0_[0x1];
        i16 localCommunicationVersion;
        u8 _pad1_[0x10];
    };
    static_assert(sizeof(NodeInfo) == 0x40);

    struct LdnNetworkInfo {
        std::array<u8, 0x10> securityParameter;
        SecurityMode securityMode;
        AcceptPolicy stationAcceptPolicy;
        u8 hasActionFrame;
        u8 _pad0_[0x2];
        u8 nodeCountMax;
        u8 nodeCount;
        std::array<NodeInfo, NodeCountMax> nodes;
        u8 _pad1_[0x2];
        u16 advertiseDataSize;
        std::array<u8, AdvertiseDataSizeMax> advertiseData;
        u8 _pad2_[0x8C];
        u64 randomAuthenticationId;
    };
    static_assert(sizeof(LdnNetworkInfo) == 0x430);

    struct NetworkInfo {
        NetworkId networkId;
        CommonNetworkInfo common;
        LdnNetworkInfo ldn;
    };
    static_assert(sizeof(NetworkInfo) == 0x480);

    struct SecurityConfig {
        SecurityMode securityMode;
        u16 passphraseSize;
        std::array<u8, PassphraseLengthMax> passphrase;
    };
    static_assert(sizeof(SecurityConfig) == 0x44);

    struct SecurityParameter {
        std::array<u8, 0x10> data;
        SessionId sessionId;
    };
    static_assert(sizeof(SecurityParameter) == 0x20);

    struct UserConfig {
        std::array<u8, UserNameBytesMax + 1> username;
        u8 _pad0_[0xF];
    };
    static_assert(sizeof(UserConfig) == 0x30);

    struct NetworkConfig {
        IntentId intentId;
        WifiChannel channel;
        u8 nodeCountMax;
        u8 _pad0_[0x1];
        u16 localCommunicationVersion;
        u8 _pad1_[0xA];
    };
    static_assert(sizeof(NetworkConfig) == 0x20);

    struct NodeLatestUpdate {
        NodeStateChange stateChange;
        u8 _pad0_[0x7];
    };
    static_assert(sizeof(NodeLatestUpdate) == 0x8);

    /**
     * @brief IUserLocalCommunicationService is used by applications to manage LDN sessions
     * @url https://switchbrew.org/wiki/LDN_services#IUserLocalCommunicationService
     */
    class IUserLocalCommunicationService : public BaseService {
      private:
        std::shared_ptr<type::KEvent> event; //!< The KEvent that is signalled on state changes
        bool isInitialized{false};

      public:
        IUserLocalCommunicationService(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#GetState
         */
        Result GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#GetNetworkInfo
         */
        Result GetNetworkInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#GetIpv4Address
         */
        Result GetIpv4Address(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#GetDisconnectReason
         */
        Result GetDisconnectReason(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#GetSecurityParameter
         */
        Result GetSecurityParameter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#GetNetworkConfig
         */
        Result GetNetworkConfig(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#AttachStateChangeEvent
         */
        Result AttachStateChangeEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#GetNetworkInfoLatestUpdate
         */
        Result GetNetworkInfoLatestUpdate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#Scan
         */
        Result Scan(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#OpenAccessPoint
         */
        Result OpenAccessPoint(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#CreateNetwork
         */
        Result CreateNetwork(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#CreateNetworkPrivate
         */
        Result CreateNetworkPrivate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#SetAdvertiseData
         */
        Result SetAdvertiseData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#OpenStation
         */
        Result OpenStation(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#InitializeSystem
         */
        Result InitializeSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#FinalizeSystem
         */
        Result FinalizeSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#InitializeSystem2
         */
        Result InitializeSystem2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

      SERVICE_DECL(
            SFUNC(0x0, IUserLocalCommunicationService, GetState),
            SFUNC(0x1, IUserLocalCommunicationService, GetNetworkInfo),
            SFUNC(0x2, IUserLocalCommunicationService, GetIpv4Address),
            SFUNC(0x3, IUserLocalCommunicationService, GetDisconnectReason),
            SFUNC(0x4, IUserLocalCommunicationService, GetSecurityParameter),
            SFUNC(0x5, IUserLocalCommunicationService, GetNetworkConfig),
            SFUNC(0x64, IUserLocalCommunicationService, AttachStateChangeEvent),
            SFUNC(0x65, IUserLocalCommunicationService, GetNetworkInfoLatestUpdate),
            SFUNC(0x66, IUserLocalCommunicationService, Scan),
            SFUNC(0xC8, IUserLocalCommunicationService, OpenAccessPoint),
            SFUNC(0xCA, IUserLocalCommunicationService, CreateNetwork),
            SFUNC(0xCB, IUserLocalCommunicationService, CreateNetworkPrivate),
            SFUNC(0xCE, IUserLocalCommunicationService, SetAdvertiseData),
            SFUNC(0x12C, IUserLocalCommunicationService, OpenStation),
            SFUNC(0x190, IUserLocalCommunicationService, InitializeSystem),
            SFUNC(0x191, IUserLocalCommunicationService, FinalizeSystem),
            SFUNC(0x192, IUserLocalCommunicationService, InitializeSystem2)
        )
    };
}
