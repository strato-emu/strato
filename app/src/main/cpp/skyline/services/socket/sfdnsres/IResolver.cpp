// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IResolver.h"
#include <arpa/inet.h>
#include <common/settings.h>

namespace skyline::service::socket {
    IResolver::IResolver(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IResolver::GetAddrInfoRequest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto [dataSize, resultCode]{GetAddrInfoRequestImpl(request)};
        response.Push<i32>(resultCode);  // errno
        response.Push(AddrInfoErrorToNetDbError(resultCode));  // NetDBErrorCode
        response.Push<u32>(dataSize);
        return {};
    }

    Result IResolver::GetHostByNameRequestWithOptions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IResolver::GetAddrInfoRequestWithOptions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto [dataSize, resultCode]{GetAddrInfoRequestImpl(request)};
        response.Push<i32>(resultCode);  // errno
        response.Push(AddrInfoErrorToNetDbError(resultCode));  // NetDBErrorCode
        response.Push<u32>(dataSize);
        response.Push<u32>(0);
        return {};
    }

    Result IResolver::GetNameInfoRequestWithOptions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    std::pair<u32, i32> IResolver::GetAddrInfoRequestImpl(ipc::IpcRequest &request) {
        auto hostname{request.inputBuf.at(0).as_string(true)};
        auto service{request.inputBuf.at(1).as_string(true)};

        if (!(*state.settings->isInternetEnabled)) {
            Logger::Info("Internet access disabled, DNS Blocked: {}", hostname);
            return {0, -1};
        }

        addrinfo* result;
        i32 resultCode = getaddrinfo(hostname.data(), service.data(), nullptr, &result);

        u32 dataSize{0};
        if (resultCode == 0 && result != nullptr) {
            const std::vector<u8>& data = SerializeAddrInfo(result, resultCode, hostname);
            dataSize = static_cast<u32>(data.size());
            request.outputBuf.at(0).copy_from(data);
            freeaddrinfo(result);
        }
        return {dataSize, resultCode};
    }

    // https://github.com/yuzu-emu/yuzu/blob/ce8f4da63834be0179d98a7720dee47d65f3ec06/src/core/hle/service/sockets/sfdnsres.cpp#L76
    std::vector<u8> IResolver::SerializeAddrInfo(const addrinfo* addrinfo, i32 result_code, std::string_view host) {
        std::vector<u8> data;

        auto* current{addrinfo};
        while (current != nullptr) {
            struct SerializedResponseHeader {
                u32 magic;
                u32 flags;
                u32 family;
                u32 socketType;
                u32 protocol;
                u32 addressLength;
            };
            static_assert(sizeof(SerializedResponseHeader) == 0x18);

            constexpr auto headerSize{sizeof(SerializedResponseHeader)};
            const auto addrSize{current->ai_addr && current->ai_addrlen > 0 ? current->ai_addrlen : 4};
            const auto canonnameSize{current->ai_canonname ? strlen(current->ai_canonname) + 1 : 1};

            const auto lastSize{data.size()};
            data.resize(lastSize + headerSize + addrSize + canonnameSize);

            // Header in network byte order
            SerializedResponseHeader header{};

            constexpr auto HEADER_MAGIC{0xBEEFCAFE};
            header.magic = htonl(HEADER_MAGIC);
            header.family = htonl(current->ai_family);
            header.flags = htonl(current->ai_flags);
            header.socketType = htonl(current->ai_socktype);
            header.protocol = htonl(current->ai_protocol);
            header.addressLength = current->ai_addr ? htonl((u32)current->ai_addrlen) : 0;

            auto* headerPtr{data.data() + lastSize};
            std::memcpy(headerPtr, &header, headerSize);

            if (header.addressLength == 0) {
                std::memset(headerPtr + headerSize, 0, 4);
            } else {
                switch (current->ai_family) {
                    case AF_INET: {
                        struct SockAddrIn {
                            u16 sin_family;
                            u16 sin_port;
                            u32 sin_addr;
                            u8 sin_zero[8];
                        };

                        SockAddrIn serializedAddr{};
                        const auto addr{*reinterpret_cast<sockaddr_in*>(current->ai_addr)};
                        serializedAddr.sin_port = htons(addr.sin_port);
                        serializedAddr.sin_family = htons(addr.sin_family);
                        serializedAddr.sin_addr = htonl(addr.sin_addr.s_addr);
                        std::memcpy(headerPtr + headerSize, &serializedAddr, sizeof(SockAddrIn));

                        char addrStringBuf[64]{};
                        inet_ntop(AF_INET, &addr.sin_addr, addrStringBuf, std::size(addrStringBuf));
                        Logger::Info("Resolved host '{}' to IPv4 address {}", host, addrStringBuf);
                        break;
                    }
                    case AF_INET6: {
                        struct SockAddrIn6 {
                            u16 sin6_family;
                            u16 sin6_port;
                            u32 sin6_flowinfo;
                            u8 sin6_addr[16];
                            u32 sin6_scope_id;
                        };

                        SockAddrIn6 serializedAddr{};
                        const auto addr{*reinterpret_cast<sockaddr_in6*>(current->ai_addr)};
                        serializedAddr.sin6_family = htons(addr.sin6_family);
                        serializedAddr.sin6_port = htons(addr.sin6_port);
                        serializedAddr.sin6_flowinfo = htonl(addr.sin6_flowinfo);
                        serializedAddr.sin6_scope_id = htonl(addr.sin6_scope_id);
                        std::memcpy(serializedAddr.sin6_addr, &addr.sin6_addr, sizeof(SockAddrIn6::sin6_addr));
                        std::memcpy(headerPtr + headerSize, &serializedAddr, sizeof(SockAddrIn6));

                        char addrStringBuf[64]{};
                        inet_ntop(AF_INET6, &addr.sin6_addr, addrStringBuf, std::size(addrStringBuf));
                        Logger::Info("Resolved host '{}' to IPv6 address {}", host, addrStringBuf);
                        break;
                    }
                    default:
                        std::memcpy(headerPtr + headerSize, current->ai_addr, addrSize);
                        break;
                }
            }
            if (current->ai_canonname) {
                std::memcpy(headerPtr + addrSize, current->ai_canonname, canonnameSize);
            } else {
                *(headerPtr + headerSize + addrSize) = 0;
            }

            current = current->ai_next;
        }

        // 4-byte sentinel value
        data.push_back(0);
        data.push_back(0);
        data.push_back(0);
        data.push_back(0);

        return data;
    }

    NetDbError IResolver::AddrInfoErrorToNetDbError(i32 result) {
        switch (result) {
            case 0:
                return NetDbError::Success;
            case EAI_AGAIN:
                return NetDbError::TryAgain;
            case EAI_NODATA:
                return NetDbError::NoData;
            default:
                return NetDbError::HostNotFound;
        }
    }
}
