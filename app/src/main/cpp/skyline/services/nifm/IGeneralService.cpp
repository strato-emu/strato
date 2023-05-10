// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IScanRequest.h"
#include "IRequest.h"
#include "IGeneralService.h"
#include <common/settings.h>
#include <jvm.h>

namespace skyline::service::nifm {
    /**
     * @brief Converts integer value to an array of bytes ordered in little-endian format
     */
    static std::array<u8, 4> ConvertIntToByteArray(i32 value) {
        std::array<u8, 4> result{};
        result[0] = value & 0xFF;
        result[1] = (value >> 8) & 0xFF;
        result[2] = (value >> 16) & 0xFF;
        result[3] = (value >> 24) & 0xFF;
        return result;
    }

    IGeneralService::IGeneralService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IGeneralService::CreateScanRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IScanRequest), session, response);
        return {};
    }

    Result IGeneralService::CreateRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IRequest), session, response);
        return {};
    }

    Result IGeneralService::GetCurrentNetworkProfile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!(*state.settings->isInternetEnabled))
            return result::NoInternetConnection;

        const UUID uuid{static_cast<u128>(0xdeadbeef) << 64};
        auto dhcpInfo{state.jvm->GetDhcpInfo()};

        SfNetworkProfileData networkProfileData{
            .ipSettingData{
                .ipAddressSetting{
                    true,
                    .currentAddress{ConvertIntToByteArray(dhcpInfo.ipAddress)},
                    .subnetMask{ConvertIntToByteArray(dhcpInfo.subnet)},
                    .gateway{ConvertIntToByteArray(dhcpInfo.gateway)},
                },
                .dnsSetting{
                    true,
                    .primaryDns{ConvertIntToByteArray(dhcpInfo.dns1)},
                    .secondaryDns{ConvertIntToByteArray(dhcpInfo.dns2)},
                },
                .proxySetting{
                    false,
                    .port{},
                    .proxyServer{},
                    .automaticAuthEnabled{},
                    .user{},
                    .password{},
                },
                1500,
            },
            .uuid{uuid},
            .networkName{"Skyline Network"},
            .wirelessSettingData{
                12,
                .ssid{"Skyline Network"},
                .passphrase{"skylinepassword"},
            },
        };

        request.outputBuf.at(0).as<SfNetworkProfileData>() = networkProfileData;
        return {};
    }

    Result IGeneralService::GetCurrentIpAddress(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!(*state.settings->isInternetEnabled))
            return result::NoInternetConnection;

        auto dhcpInfo{state.jvm->GetDhcpInfo()};
        response.Push(ConvertIntToByteArray(dhcpInfo.ipAddress));
        return {};
    }

    Result IGeneralService::GetCurrentIpConfigInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!(*state.settings->isInternetEnabled))
            return result::NoInternetConnection;

        auto dhcpInfo{state.jvm->GetDhcpInfo()};

        struct IpConfigInfo {
            IpAddressSetting ipAddressSetting;
            DnsSetting dnsSetting;
        };

        IpConfigInfo ipConfigInfo{
            .ipAddressSetting{
                true,
                .currentAddress{ConvertIntToByteArray(dhcpInfo.ipAddress)},
                .subnetMask{ConvertIntToByteArray(dhcpInfo.subnet)},
                .gateway{ConvertIntToByteArray(dhcpInfo.gateway)},
            },
            .dnsSetting{
                true,
                .primaryDns{ConvertIntToByteArray(dhcpInfo.dns1)},
                .secondaryDns{ConvertIntToByteArray(dhcpInfo.dns2)},
            },
        };

        response.Push(ipConfigInfo);
        return {};
    }

    Result IGeneralService::GetInternetConnectionStatus(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct Status {
            u8 type{1};
            u8 wifiStrength{3};
            u8 state{4};
        } status{};
        response.Push(status);
        return {};
    }

    Result IGeneralService::IsAnyInternetRequestAccepted(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(*state.settings->isInternetEnabled);
        return {};
    }
}
