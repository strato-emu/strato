// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include "common/uuid.h"

namespace skyline::service::nifm {
    namespace result {
        constexpr Result NoInternetConnection{110, 300};
    }

    struct IpAddressSetting {
        bool isAutomatic{};
        std::array<u8, 4> currentAddress{};
        std::array<u8, 4> subnetMask{};
        std::array<u8, 4> gateway{};
    };
    static_assert(sizeof(IpAddressSetting) == 0xD);

    struct DnsSetting {
        bool isAutomatic{};
        std::array<u8, 4> primaryDns{};
        std::array<u8, 4> secondaryDns{};
    };
    static_assert(sizeof(DnsSetting) == 0x9);

    struct ProxySetting {
        bool enabled{};
        u8 _pad0_[0x1];
        u16 port{};
        std::array<char, 0x64> proxyServer{};
        bool automaticAuthEnabled{};
        std::array<char, 0x20> user{};
        std::array<char, 0x20> password{};
        u8 _pad1_[0x1];
    };
    static_assert(sizeof(ProxySetting) == 0xAA);

    struct IpSettingData {
        IpAddressSetting ipAddressSetting{};
        DnsSetting dnsSetting{};
        ProxySetting proxySetting{};
        u16 mtu{};
    };
    static_assert(sizeof(IpSettingData) == 0xC2);

    struct SfWirelessSettingData {
        u8 ssidLength{};
        std::array<char, 0x20> ssid{};
        u8 _unk0_[0x3];
        std::array<char, 0x41> passphrase{};
    };
    static_assert(sizeof(SfWirelessSettingData) == 0x65);

    struct NifmWirelessSettingData {
        u8 ssidLength{};
        std::array<char, 0x21> ssid{};
        u8 _unk0_[0x1];
        u8 _pad0_[0x1];
        u32 _unk1_[0x2];
        std::array<char, 0x41> passphrase{};
        u8 _pad1_[0x3];
    };
    static_assert(sizeof(NifmWirelessSettingData) == 0x70);

    #pragma pack(push, 1)
    struct SfNetworkProfileData {
        IpSettingData ipSettingData{};
        UUID uuid{};
        std::array<char, 0x40> networkName{};
        u8 _unk0_[0x4];
        SfWirelessSettingData wirelessSettingData{};
        u8 _pad0_[0x1];
    };
    static_assert(sizeof(SfNetworkProfileData) == 0x17C);

    struct NifmNetworkProfileData {
        UUID uuid{};
        std::array<char, 0x40> networkName{};
        u32 _unk0_[0x2];
        u8 _unk1_[0x2];
        u8 _pad0_[0x2];
        NifmWirelessSettingData wirelessSettingData{};
        IpSettingData ipSettingData{};
    };
    static_assert(sizeof(NifmNetworkProfileData) == 0x18E);
    #pragma pack(pop)

    /**
     * @brief IGeneralService is used by applications to control the network connection
     * @url https://switchbrew.org/wiki/Network_Interface_services#IGeneralService
     */
    class IGeneralService : public BaseService {
      public:
        IGeneralService(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/Network_Interface_services#CreateScanRequest
         */
        Result CreateScanRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Network_Interface_services#CreateRequest
         */
        Result CreateRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Network_Interface_services#GetCurrentNetworkProfile
         */
        Result GetCurrentNetworkProfile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Network_Interface_services#GetCurrentIpAddress
         */
        Result GetCurrentIpAddress(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Network_Interface_services#GetCurrentIpConfigInfo
         */
        Result GetCurrentIpConfigInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetInternetConnectionStatus(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Network_Interface_services#IsAnyInternetRequestAccepted
         */
        Result IsAnyInternetRequestAccepted(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x1, IGeneralService, CreateScanRequest),
            SFUNC(0x4, IGeneralService, CreateRequest),
            SFUNC(0x5, IGeneralService, GetCurrentNetworkProfile),
            SFUNC(0xC, IGeneralService, GetCurrentIpAddress),
            SFUNC(0xF, IGeneralService, GetCurrentIpConfigInfo),
            SFUNC(0x12, IGeneralService, GetInternetConnectionStatus),
            SFUNC(0x15, IGeneralService, IsAnyInternetRequestAccepted)
        )
    };
}
