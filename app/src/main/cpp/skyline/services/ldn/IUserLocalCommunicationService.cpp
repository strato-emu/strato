// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IUserLocalCommunicationService.h"
#include <common/settings.h>

namespace skyline::service::ldn {
    IUserLocalCommunicationService::IUserLocalCommunicationService(const DeviceState &state, ServiceManager &manager)
        : BaseService(state, manager),
          event{std::make_shared<type::KEvent>(state, false)} {}

    Result IUserLocalCommunicationService::GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(State::Error);
        return {};
    }

    Result IUserLocalCommunicationService::GetNetworkInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (request.outputBuf.at(0).size() != sizeof(NetworkInfo)) {
            Logger::Error("Invalid input");
            return result::InvalidInput;
        }

        NetworkInfo networkInfo{};
        request.outputBuf.at(0).as<NetworkInfo>() = networkInfo;
        return {};
    }

    Result IUserLocalCommunicationService::GetIpv4Address(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IUserLocalCommunicationService::GetDisconnectReason(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(DisconnectReason::None);
        return {};
    }

    Result IUserLocalCommunicationService::GetSecurityParameter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        SecurityParameter securityParameter{};
        response.Push(securityParameter);
        return {};
    }

    Result IUserLocalCommunicationService::GetNetworkConfig(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        NetworkConfig networkConfig{};
        response.Push(networkConfig);
        return {};
    }

    Result IUserLocalCommunicationService::AttachStateChangeEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(event)};
        Logger::Debug("LDN State Change Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IUserLocalCommunicationService::GetNetworkInfoLatestUpdate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        const size_t networkBuffferSize{request.outputBuf.at(0).size()};
        const size_t nodeBufferCount{request.outputBuf.at(1).size() / sizeof(NodeLatestUpdate)};

        if (nodeBufferCount == 0 || networkBuffferSize != sizeof(NetworkInfo))
            return result::InvalidInput;

        NetworkInfo networkInfo{};
        std::vector<NodeLatestUpdate> latestUpdate(nodeBufferCount);

        request.outputBuf.at(0).as<NetworkInfo>() = networkInfo;
        request.outputBuf.at(1).copy_from(latestUpdate);
        return {};
    }

    Result IUserLocalCommunicationService::Scan(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        const size_t networkInfoSize{request.outputBuf.at(0).size() / sizeof(NetworkInfo)};

        if (networkInfoSize == 0)
            return result::InvalidInput;

        std::vector<NetworkInfo> networkInfos(networkInfoSize);
        request.outputBuf.at(0).copy_from(networkInfos);
        response.Push<u32>(0);
        return {};
    }

    Result IUserLocalCommunicationService::OpenAccessPoint(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IUserLocalCommunicationService::CreateNetwork(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IUserLocalCommunicationService::CreateNetworkPrivate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IUserLocalCommunicationService::SetAdvertiseData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IUserLocalCommunicationService::OpenStation(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IUserLocalCommunicationService::InitializeSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!*state.settings->isInternetEnabled)
            return result::AirplaneModeEnabled;

        isInitialized = true;
        return result::AirplaneModeEnabled;
    }

    Result IUserLocalCommunicationService::FinalizeSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        isInitialized = false;
        return {};
    }

    Result IUserLocalCommunicationService::InitializeSystem2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!*state.settings->isInternetEnabled)
            return result::AirplaneModeEnabled;

        isInitialized = true;
        return result::AirplaneModeEnabled;
    }
}
