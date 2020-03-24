#include <gpu.h>
#include <os.h>
#include <kernel/types/KProcess.h>
#include <services/nvdrv/INvDrvServices.h>
#include "IHOSBinderDriver.h"
#include "display.h"

namespace skyline::service::hosbinder {
    IHOSBinderDriver::IHOSBinderDriver(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::hosbinder_IHOSBinderDriver, "hosbinder_IHOSBinderDriver", {
        {0x0, SFUNC(IHOSBinderDriver::TransactParcel)},
        {0x1, SFUNC(IHOSBinderDriver::AdjustRefcount)},
        {0x2, SFUNC(IHOSBinderDriver::GetNativeHandle)},
        {0x3, SFUNC(IHOSBinderDriver::TransactParcel)}
    }) {}

    void IHOSBinderDriver::RequestBuffer(Parcel &in, Parcel &out) {
        u32 slot = *reinterpret_cast<u32 *>(in.data.data() + constant::TokenLength);
        out.WriteData<u32>(1);
        out.WriteData<u32>(sizeof(GbpBuffer));
        out.WriteData<u32>(0);
        out.WriteData(queue.at(slot)->gbpBuffer);
        state.logger->Debug("RequestBuffer: Slot: {}", slot, sizeof(GbpBuffer));
    }

    void IHOSBinderDriver::DequeueBuffer(Parcel &in, Parcel &out) {
        struct Data {
            u32 format;
            u32 width;
            u32 height;
            u32 timestamps;
            u32 usage;
        } *data = reinterpret_cast<Data *>(in.data.data() + constant::TokenLength);

        i64 slot{-1};
        while (slot == -1) {
            for (auto &buffer : queue) {
                if (buffer.second->status == BufferStatus::Free && buffer.second->gbpBuffer.width == data->width && buffer.second->gbpBuffer.height == data->height && buffer.second->gbpBuffer.usage == data->usage) {
                    slot = buffer.first;
                    buffer.second->status = BufferStatus::Dequeued;
                    break;
                }
            }
            asm("yield");
        }

        struct {
            u32 slot;
            u32 _unk_[13];
        } output{
            .slot = static_cast<u32>(slot)
        };
        out.WriteData(output);

        state.logger->Debug("DequeueBuffer: Width: {}, Height: {}, Format: {}, Usage: {}, Timestamps: {}, Slot: {}", data->width, data->height, data->format, data->usage, data->timestamps, slot);
    }

    void IHOSBinderDriver::QueueBuffer(Parcel &in, Parcel &out) {
        struct Data {
            u32 slot;
            u64 timestamp;
            u32 autoTimestamp;
            ARect crop;
            u32 scalingMode;
            u32 transform;
            u32 stickyTransform;
            u64 _unk0_;
            u32 swapInterval;
            Fence fence[4];
        } *data = reinterpret_cast<Data *>(in.data.data() + constant::TokenLength);

        auto buffer = queue.at(data->slot);
        buffer->status = BufferStatus::Queued;

        auto slot = data->slot;
        auto bufferEvent = state.gpu->bufferEvent;
        buffer->texture->releaseCallback = [this, slot, bufferEvent]() {
            FreeBuffer(slot);
            bufferEvent->Signal();
        };

        buffer->texture->SynchronizeHost();

        state.gpu->presentationQueue.push(buffer->texture);

        struct {
            u32 width;
            u32 height;
            u32 _pad0_[3];
        } output{
            .width = buffer->gbpBuffer.width,
            .height = buffer->gbpBuffer.height
        };

        out.WriteData(output);

        state.logger->Debug("QueueBuffer: Timestamp: {}, Auto Timestamp: {}, Crop: [T: {}, B: {}, L: {}, R: {}], Scaling Mode: {}, Transform: {}, Sticky Transform: {}, Swap Interval: {}, Slot: {}", data->timestamp, data->autoTimestamp, data->crop.top, data->crop.bottom, data->crop.left, data->crop.right, data->scalingMode, data->transform, data->stickyTransform, data->swapInterval, data->slot);
    }

    void IHOSBinderDriver::CancelBuffer(Parcel &parcel) {
        struct Data {
            u32 slot;
            Fence fence[4];
        } *data = reinterpret_cast<Data *>(parcel.data.data() + constant::TokenLength);
        FreeBuffer(data->slot);
        state.logger->Debug("CancelBuffer: Slot: {}", data->slot);
    }

    void IHOSBinderDriver::SetPreallocatedBuffer(Parcel &parcel) {
        auto pointer = parcel.data.data() + constant::TokenLength;
        struct Data {
            u32 slot;
            u32 _unk0_;
            u32 length;
            u32 _pad0_;
        } *data = reinterpret_cast<Data *>(pointer);

        pointer += sizeof(Data);
        auto gbpBuffer = reinterpret_cast<GbpBuffer *>(pointer);

        std::shared_ptr<nvdrv::device::NvMap::NvMapObject> nvBuffer{};
        auto nvmap = state.os->serviceManager.GetService<nvdrv::INvDrvServices>(Service::nvdrv_INvDrvServices)->GetDevice<nvdrv::device::NvMap>(nvdrv::device::NvDeviceType::nvmap);

        if (gbpBuffer->nvmapHandle)
            nvBuffer = nvmap->handleTable.at(gbpBuffer->nvmapHandle);
        else {
            for (const auto &object : nvmap->handleTable) {
                if (object.second->id == gbpBuffer->nvmapId) {
                    nvBuffer = object.second;
                    break;
                }
            }
            if (!nvBuffer)
                throw exception("A QueueBuffer request has an invalid NVMap Handle ({}) and ID ({})", gbpBuffer->nvmapHandle, gbpBuffer->nvmapId);
        }

        gpu::texture::Format format;
        switch (gbpBuffer->format) {
            case WINDOW_FORMAT_RGBA_8888:
            case WINDOW_FORMAT_RGBX_8888:
                format = gpu::texture::format::RGBA8888Unorm;
                break;
            case WINDOW_FORMAT_RGB_565:
                format = gpu::texture::format::RGB565Unorm;
                break;
            default:
                throw exception("Unknown pixel format used for FB");
        }

        auto texture = std::make_shared<gpu::GuestTexture>(state, nvBuffer->address + gbpBuffer->offset, gpu::texture::Dimensions(gbpBuffer->width, gbpBuffer->height), format, gpu::texture::TileMode::Block, gpu::texture::TileConfig{.surfaceWidth = static_cast<u16>(gbpBuffer->stride), .blockHeight = static_cast<u8>(1U << gbpBuffer->blockHeightLog2), .blockDepth = 1});

        queue[data->slot] = std::make_shared<Buffer>(data->slot, *gbpBuffer, texture->InitializePresentationTexture());
        state.gpu->bufferEvent->Signal();

        state.logger->Debug("SetPreallocatedBuffer: Slot: {}, Magic: 0x{:X}, Width: {}, Height: {}, Stride: {}, Format: {}, Usage: {}, Index: {}, ID: {}, Handle: {}, Offset: 0x{:X}, Block Height: {}, Size: 0x{:X}", data->slot, gbpBuffer->magic, gbpBuffer->width, gbpBuffer->height, gbpBuffer->stride, gbpBuffer->format, gbpBuffer->usage, gbpBuffer->index, gbpBuffer->nvmapId,
                            gbpBuffer->nvmapHandle, gbpBuffer->offset, (1U << gbpBuffer->blockHeightLog2), gbpBuffer->size);
    }

    void IHOSBinderDriver::TransactParcel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto layerId = request.Pop<u32>();
        auto code = request.Pop<TransactionCode>();

        Parcel in(request.inputBuf.at(0), state);
        Parcel out(state);

        state.logger->Debug("TransactParcel: Layer ID: {}, Code: {}", layerId, code);

        switch (code) {
            case TransactionCode::RequestBuffer:
                RequestBuffer(in, out);
                break;
            case TransactionCode::DequeueBuffer:
                DequeueBuffer(in, out);
                break;
            case TransactionCode::QueueBuffer:
                QueueBuffer(in, out);
                break;
            case TransactionCode::CancelBuffer:
                CancelBuffer(in);
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
                SetPreallocatedBuffer(in);
                break;
            default:
                throw exception("An unimplemented transaction was called: {}", static_cast<u32>(code));
        }

        out.WriteParcel(request.outputBuf.at(0));
    }

    void IHOSBinderDriver::AdjustRefcount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.Skip<u32>();
        auto addVal = request.Pop<i32>();
        auto type = request.Pop<i32>();
        state.logger->Debug("Reference Change: {} {} reference", addVal, type ? "strong" : "weak");
    }

    void IHOSBinderDriver::GetNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        handle_t handle = state.process->InsertItem(state.gpu->bufferEvent);
        state.logger->Debug("Display Buffer Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        response.Push<u32>(constant::status::Success);
    }

    void IHOSBinderDriver::SetDisplay(const std::string &name) {
        try {
            const auto type = displayTypeMap.at(name);
            if (displayId == DisplayId::Null)
                displayId = type;
            else
                throw exception("Trying to change display type from non-null type");
        } catch (const std::out_of_range &) {
            throw exception("The display with name: '{}' doesn't exist", name);
        }
    }

    void IHOSBinderDriver::CloseDisplay() {
        if (displayId == DisplayId::Null)
            state.logger->Warn("Trying to close uninitiated display");
        displayId = DisplayId::Null;
    }
}
