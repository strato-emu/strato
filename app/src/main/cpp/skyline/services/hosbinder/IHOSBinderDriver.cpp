// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <kernel/types/KProcess.h>
#include "IHOSBinderDriver.h"
#include "GraphicBufferProducer.h"

namespace skyline::service::hosbinder {
    IHOSBinderDriver::IHOSBinderDriver(const DeviceState &state, ServiceManager &manager) : producer(hosbinder::producer.expired() ? std::make_shared<GraphicBufferProducer>(state) : hosbinder::producer.lock()), BaseService(state, manager) {
        if (hosbinder::producer.expired())
            hosbinder::producer = producer;
    }

    Result IHOSBinderDriver::TransactParcel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto layerId = request.Pop<u32>();
        auto code = request.Pop<GraphicBufferProducer::TransactionCode>();

        Parcel in(request.inputBuf.at(0), state, true);
        Parcel out(state);

        state.logger->Debug("TransactParcel: Layer ID: {}, Code: {}", layerId, code);
        producer->OnTransact(code, in, out);

        out.WriteParcel(request.outputBuf.at(0));
        return {};
    }

    Result IHOSBinderDriver::AdjustRefcount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.Skip<u32>();
        auto addVal = request.Pop<i32>();
        auto type = request.Pop<i32>();
        state.logger->Debug("Reference Change: {} {} reference", addVal, type ? "strong" : "weak");

        return {};
    }

    Result IHOSBinderDriver::GetNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        KHandle handle = state.process->InsertItem(state.gpu->bufferEvent);
        state.logger->Debug("Display Buffer Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);

        return {};
    }
}
