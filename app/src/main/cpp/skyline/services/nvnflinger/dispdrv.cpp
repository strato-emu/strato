#include "dispdrv.h"
#include <kernel/types/KProcess.h>
#include <gpu.h>
#include <android/native_window.h>

namespace skyline::service::nvnflinger {
    dispdrv::dispdrv(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::nvnflinger_dispdrv, {
        {0x0, SFUNC(dispdrv::TransactParcel)},
        {0x1, SFUNC(dispdrv::AdjustRefcount)},
        {0x2, SFUNC(dispdrv::GetNativeHandle)},
        {0x3, SFUNC(dispdrv::TransactParcel)}
    }) {}

    void dispdrv::TransactParcel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct InputStruct {
            u32 layerId;
            TransactionCode code;
        } *input = reinterpret_cast<InputStruct *>(request.cmdArg);
        std::shared_ptr<gpu::Parcel> in{};
        if (request.vecBufA.empty())
            in = std::make_shared<gpu::Parcel>(request.vecBufX[0], state);
        else
            in = std::make_shared<gpu::Parcel>(request.vecBufA[0], state);
        gpu::Parcel out(state);
        state.logger->Debug("TransactParcel: Layer ID: {}, Code: {}", input->layerId, input->code);
        switch (input->code) {
            case TransactionCode::RequestBuffer:
                state.gpu->bufferQueue.RequestBuffer(*in, out);
                break;
            case TransactionCode::DequeueBuffer: {
                if (request.vecBufA.empty()) {
                    if (state.gpu->bufferQueue.DequeueBuffer(*in, out, request.vecBufC[0]->address, request.vecBufC[0]->size))
                        return;
                } else {
                    if (state.gpu->bufferQueue.DequeueBuffer(*in, out, request.vecBufB[0]->Address(), request.vecBufB[0]->Size()))
                        return;
                }
                break;
            }
            case TransactionCode::QueueBuffer:
                state.gpu->bufferQueue.QueueBuffer(*in, out);
                break;
            case TransactionCode::CancelBuffer:
                state.gpu->bufferQueue.CancelBuffer(*in);
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
                state.gpu->bufferQueue.SetPreallocatedBuffer(*in);
                break;
            default:
                throw exception("An unimplemented transaction was called: {}", static_cast<u32>(input->code));
        }
        if (request.vecBufA.empty())
            out.WriteParcel(request.vecBufC[0]);
        else
            out.WriteParcel(request.vecBufB[0]);
    }

    void dispdrv::GetNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        handle_t handle = state.thisProcess->InsertItem(state.gpu->bufferEvent);
        state.logger->Debug("BufferEvent Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        response.WriteValue<u32>(constant::status::Success);
    }

    void dispdrv::AdjustRefcount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct InputStruct {
            u32 layerId;
            i32 addVal;
            i32 type;
        } *input = reinterpret_cast<InputStruct *>(request.cmdArg);
        state.logger->Debug("Reference Change: {} {} reference", input->addVal, input->type ? "strong" : "weak");
    }
}
