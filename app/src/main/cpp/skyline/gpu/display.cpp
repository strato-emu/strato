#include "display.h"
#include <kernel/types/KProcess.h>
#include <gpu.h>

namespace skyline::gpu {
    Buffer::Buffer(const DeviceState &state, u32 slot, GbpBuffer &gbpBuffer) : state(state), slot(slot), gbpBuffer(gbpBuffer), resolution{gbpBuffer.width, gbpBuffer.height}, dataBuffer(gbpBuffer.size) {
        if (gbpBuffer.nvmapHandle)
            nvBuffer = state.gpu->GetDevice<device::NvMap>(device::NvDeviceType::nvmap)->handleTable.at(gbpBuffer.nvmapHandle);
        else {
            auto nvmap = state.gpu->GetDevice<device::NvMap>(device::NvDeviceType::nvmap);
            for (const auto &object : nvmap->handleTable) {
                if (object.second->id == gbpBuffer.nvmapId) {
                    nvBuffer = object.second;
                    break;
                }
            }
            if (!nvBuffer)
                throw exception("A QueueBuffer request has an invalid NVMap Handle ({}) and ID ({})", gbpBuffer.nvmapHandle, gbpBuffer.nvmapId);
        }
        switch(gbpBuffer.format) {
            case WINDOW_FORMAT_RGBA_8888:
            case WINDOW_FORMAT_RGBX_8888:
                bpp = sizeof(u32);
                break;
            case WINDOW_FORMAT_RGB_565:
                bpp = sizeof(u16);
                break;
            default:
                throw exception("Unknown pixel format used for FB");
        }
    }

    void Buffer::UpdateBuffer() {
        state.process->ReadMemory(dataBuffer.data(), nvBuffer->address + gbpBuffer.offset, gbpBuffer.size);
    }

    BufferQueue::WaitContext::WaitContext(std::shared_ptr<kernel::type::KThread> thread, DequeueIn input, kernel::ipc::OutputBuffer& buffer) : thread(std::move(thread)), input(input), buffer(buffer) {}

    BufferQueue::DequeueOut::DequeueOut(u32 slot) : slot(slot), _unk0_(0x1), _unk1_(0x24) {}

    BufferQueue::BufferQueue(const DeviceState &state) : state(state) {}

    void BufferQueue::RequestBuffer(Parcel &in, Parcel &out) {
        u32 slot = *reinterpret_cast<u32 *>(in.data.data() + constant::TokenLength);
        out.WriteData<u32>(1);
        out.WriteData<u32>(sizeof(GbpBuffer));
        out.WriteData<u32>(0);
        out.WriteData(queue.at(slot)->gbpBuffer);
        state.logger->Debug("RequestBuffer: Slot: {}", slot, sizeof(GbpBuffer));
    }

    void BufferQueue::DequeueBuffer(Parcel &in, Parcel &out) {
        auto *data = reinterpret_cast<DequeueIn *>(in.data.data() + constant::TokenLength);
        i64 slot{-1};
        while(slot == -1) {
            for (auto &buffer : queue) {
                if (buffer.second->status == BufferStatus::Free && buffer.second->resolution.width == data->width && buffer.second->resolution.height == data->height && buffer.second->gbpBuffer.usage == data->usage) {
                    slot = buffer.first;
                    buffer.second->status = BufferStatus::Dequeued;
                    break;
                }
            }
            sched_yield();
        }
        DequeueOut output(static_cast<u32>(slot));
        out.WriteData(output);
        state.logger->Debug("DequeueBuffer: Width: {}, Height: {}, Format: {}, Usage: {}, Timestamps: {}, Slot: {}", data->width, data->height, data->format, data->usage, data->timestamps, slot);
    }

    void BufferQueue::QueueBuffer(Parcel &in, Parcel &out) {
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
        buffer->UpdateBuffer();
        displayQueue.emplace(buffer);
        state.gpu->bufferEvent->Signal();
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

    void BufferQueue::CancelBuffer(Parcel &parcel) {
        struct Data {
            u32 slot;
            Fence fence[4];
        } *data = reinterpret_cast<Data *>(parcel.data.data() + constant::TokenLength);
        FreeBuffer(data->slot);
        state.logger->Debug("CancelBuffer: Slot: {}", data->slot);
    }

    void BufferQueue::SetPreallocatedBuffer(Parcel &parcel) {
        auto pointer = parcel.data.data() + constant::TokenLength;
        struct Data {
            u32 slot;
            u32 _unk0_;
            u32 length;
            u32 _pad0_;
        } *data = reinterpret_cast<Data *>(pointer);
        pointer += sizeof(Data);
        auto gbpBuffer = reinterpret_cast<GbpBuffer *>(pointer);
        queue[data->slot] = std::make_shared<Buffer>(state, data->slot, *gbpBuffer);
        state.gpu->bufferEvent->Signal();
        state.logger->Debug("SetPreallocatedBuffer: Slot: {}, Magic: 0x{:X}, Width: {}, Height: {}, Stride: {}, Format: {}, Usage: {}, Index: {}, ID: {}, Handle: {}, Offset: 0x{:X}, Block Height: {}, Size: 0x{:X}", data->slot, gbpBuffer->magic, gbpBuffer->width, gbpBuffer->height, gbpBuffer->stride, gbpBuffer->format, gbpBuffer->usage, gbpBuffer->index,gbpBuffer->nvmapId, gbpBuffer->nvmapHandle, gbpBuffer->offset, (1U << gbpBuffer->blockHeightLog2), gbpBuffer->size);
    }

    void BufferQueue::FreeBuffer(u32 slotNo) {
        auto &slot = queue.at(slotNo);
        if (waitVec.empty())
            slot->status = BufferStatus::Free;
        else {
            auto context = waitVec.begin();
            while (context != waitVec.end()) {
                if (slot->resolution.width == context->input.width && slot->resolution.height == context->input.height && slot->gbpBuffer.usage == context->input.usage) {
                    context->thread->WakeUp();
                    gpu::Parcel out(state);
                    DequeueOut output(slotNo);
                    out.WriteData(output);
                    out.WriteParcel(context->buffer);
                    slot->status = BufferStatus::Dequeued;
                    waitVec.erase(context);
                    break;
                }
                context++;
            }
        }
    }
}
