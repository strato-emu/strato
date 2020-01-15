#include "dispdrv.h"
#include <kernel/types/KProcess.h>
#include <gpu.h>

namespace skyline::service::nvnflinger {
    dispdrv::dispdrv(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::nvnflinger_dispdrv, {
        {0x0, SFUNC(dispdrv::TransactParcel)},
        {0x1, SFUNC(dispdrv::AdjustRefcount)},
        {0x2, SFUNC(dispdrv::GetNativeHandle)},
        {0x3, SFUNC(dispdrv::TransactParcel)}
    }) {}

    void dispdrv::TransactParcel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto layerId = request.Pop<u32>();
        auto code = request.Pop<TransactionCode>();
        gpu::Parcel in(request.inputBuf.at(0), state);
        gpu::Parcel out(state);
        state.logger->Debug("TransactParcel: Layer ID: {}, Code: {}", layerId, code);
        switch (code) {
            case TransactionCode::RequestBuffer:
                state.gpu->bufferQueue.RequestBuffer(in, out);
                break;
            case TransactionCode::DequeueBuffer:
                state.gpu->bufferQueue.DequeueBuffer(in, out);
                break;
            case TransactionCode::QueueBuffer:
                state.gpu->bufferQueue.QueueBuffer(in, out);
                break;
            case TransactionCode::CancelBuffer:
                state.gpu->bufferQueue.CancelBuffer(in);
                break;
            case TransactionCode::Query:
                out.WriteData<u64>(0);
                break;
            case TransactionCode::Connect: {
                ConnectParcel connect = {
                    .width = constant::HandheldResolutionW,
                    .height = constant::HandheldResolutionH,
                    .transformHint = 0,
                    .pendingBuffers = 0,
                    .status = constant::status::Success,
                };
                out.WriteData(connect);
                break;
            }
            case TransactionCode::Disconnect:
                break;
            case TransactionCode::SetPreallocatedBuffer:
                state.gpu->bufferQueue.SetPreallocatedBuffer(in);
                break;
            default:
                throw exception("An unimplemented transaction was called: {}", static_cast<u32>(code));
        }
        out.WriteParcel(request.outputBuf.at(0));
    }

    void dispdrv::AdjustRefcount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.Skip<u32>();
        auto addVal = request.Pop<i32>();
        auto type = request.Pop<i32>();
        state.logger->Debug("Reference Change: {} {} reference", addVal, type ? "strong" : "weak");
    }

    void dispdrv::GetNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        handle_t handle = state.process->InsertItem(state.gpu->bufferEvent);
        state.logger->Debug("Display Buffer Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        response.Push<u32>(constant::status::Success);
    }
}
