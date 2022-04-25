// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IRequest.h"

namespace skyline::service::mmnv {
    IRequest::IRequest(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    IRequest::Allocation IRequest::AllocateRequest() {
        // Search existing requests first
        for (u32 i{}; i < requests.size(); i++) {
            auto &req = requests[i];
            if (!req) {
                return {req.emplace(), i};
            }
        }

        // Allocate a new request otherwise
        return {requests.emplace_back().emplace(), static_cast<u32>(requests.size() - 1)};
    }

    Result IRequest::InitializeOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto module{request.Pop<ModuleType>()};
        request.Skip<u32>(); // Unknown unused param in HOS
        //u32 autoClear{request.Pop<u32>()};

        std::scoped_lock lock{requestsMutex};
        AllocateRequest().request.module = module;

        return {};
    }

    Result IRequest::FinalizeOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto module{request.Pop<ModuleType>()};

        std::scoped_lock lock{requestsMutex};
        for (auto &req : requests) {
            if (req && req->module == module) {
                req.reset();
                break;
            }
        }

        return {};
    }

    Result IRequest::SetAndWaitOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto module{request.Pop<ModuleType>()};
        u32 freqHz{request.Pop<u32>()};

        std::scoped_lock lock{requestsMutex};
        for (auto &req : requests) {
            if (req && req->module == module) {
                req->freqHz = freqHz;
                Logger::Debug("Set frequency for module {}: {} Hz", static_cast<u32>(module), freqHz);
                return {};
            }
        }

        // This doesn't return any errors in HOS
        Logger::Warn("Tried to set frequency to {} Hz for unregistered module {}", freqHz,  static_cast<u32>(module));

        return {};
    }

    Result IRequest::GetOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto module{request.Pop<ModuleType>()};

        std::scoped_lock lock{requestsMutex};
        for (auto &req : requests) {
            if (req && req->module == module) {
                Logger::Debug("Get frequency for module {}: {} Hz", static_cast<u32>(module), req->freqHz);
                response.Push<u32>(req->freqHz);
                return {};
            }
        }

        // This doesn't return any errors in HOS
        Logger::Warn("Tried to get frequency of unregistered module {}", static_cast<u32>(module));
        response.Push<u32>(0);
        return {};
    }

    Result IRequest::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto module{request.Pop<ModuleType>()};
        request.Skip<u32>(); // Unknown unused param in HOS
        //u32 autoClear{request.Pop<u32>()};

        std::scoped_lock lock{requestsMutex};
        auto req{AllocateRequest()};
        req.request.module = module;
        response.Push<u32>(req.id);
        return {};
    }

    Result IRequest::Finalize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 id{request.Pop<u32>()};

        std::scoped_lock lock{requestsMutex};
        if (id >= requests.size())
            return {};

        requests[id].reset();
        return {};
    };

    Result IRequest::SetAndWait(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 id{request.Pop<u32>()};
        u32 freqHz{request.Pop<u32>()};

        std::scoped_lock lock{requestsMutex};
        if (id < requests.size()) {
            auto &req{requests[id]};
            if (req) {
                req->freqHz = freqHz;
                Logger::Debug("Set frequency for request {}: {} Hz", id, freqHz);
                return {};
            }
        }

        // This doesn't return any errors in HOS
        Logger::Warn("Tried to set frequency for unregistered request {}", id);
        return {};
    }

    Result IRequest::Get(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 id{request.Pop<u32>()};

        std::scoped_lock lock{requestsMutex};
        if (id < requests.size()) {
            auto &req{requests[id]};
            if (req) {
                Logger::Debug("Get frequency for request {}: {} Hz", id, req->freqHz);
                response.Push<u32>(req->freqHz);
                return {};
            }
        }

        // This doesn't return any errors in HOS
        Logger::Warn("Tried to get frequency of unregistered request {}", id);
        response.Push<u32>(0);
        return {};
    }
}
