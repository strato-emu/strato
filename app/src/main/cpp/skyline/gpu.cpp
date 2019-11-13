#include "gpu.h"
#include "gpu/devices/nvhost_ctrl.h"
#include "gpu/devices/nvhost_ctrl_gpu.h"
#include "gpu/devices/nvhost_channel.h"
#include "gpu/devices/nvhost_as_gpu.h"
#include <kernel/types/KProcess.h>

extern bool Halt;

namespace skyline::gpu {
    GPU::GPU(const DeviceState &state, ANativeWindow *window) : state(state), window(window), bufferQueue(state), vsyncEvent(std::make_shared<kernel::type::KEvent>(state)), bufferEvent(std::make_shared<kernel::type::KEvent>(state)) {
        ANativeWindow_acquire(window);
        resolution.width = static_cast<u32>(ANativeWindow_getWidth(window));
        resolution.height = static_cast<u32>(ANativeWindow_getHeight(window));
    }

    GPU::~GPU() {
        ANativeWindow_release(window);
    }

    void GPU::Loop() {
        if (!bufferQueue.displayQueue.empty()) {
            auto &buffer = bufferQueue.displayQueue.front();
            bufferQueue.displayQueue.pop();
            if (resolution != buffer->resolution || buffer->gbpBuffer.format != format) {
                if (resolution != buffer->resolution && buffer->gbpBuffer.format != format) {
                    ANativeWindow_setBuffersGeometry(window, buffer->resolution.width, buffer->resolution.height, buffer->gbpBuffer.format);
                    resolution = buffer->resolution;
                    format = buffer->gbpBuffer.format;
                } else if (resolution != buffer->resolution) {
                    ANativeWindow_setBuffersGeometry(window, buffer->resolution.width, buffer->resolution.height, format);
                    resolution = buffer->resolution;
                } else if (buffer->gbpBuffer.format != format) {
                    ANativeWindow_setBuffersGeometry(window, resolution.width, resolution.height, buffer->gbpBuffer.format);
                    format = buffer->gbpBuffer.format;
                }
            }
            buffer->UpdateBuffer();
            auto bufferData = buffer->dataBuffer.data();
            //madvise(bufferData, buffer->gbpBuffer.size, MADV_SEQUENTIAL); (Uncomment this after deswizzling while reading sequentially instead of writing sequentially)
            ANativeWindow_Buffer windowBuffer;
            ARect rect;
            ANativeWindow_lock(window, &windowBuffer, &rect);
            u32 *address = reinterpret_cast<u32 *>(windowBuffer.bits);
            for (u32 y = 0; y < buffer->resolution.height; y++) {
                for (u32 x = 0; x < buffer->resolution.width; x += 4, address += 4) {
                    u32 position = (y & 0x7f) >> 4U;
                    position += (x >> 4U) << 3U;
                    position += (y >> 7U) * ((resolution.width >> 4U) << 3U);
                    position *= 1024;
                    position += ((y & 0xf) >> 3U) << 9U;
                    position += ((x & 0xf) >> 3U) << 8U;
                    position += ((y & 0x7) >> 1U) << 6U;
                    position += ((x & 0x7) >> 2U) << 5U;
                    position += (y & 0x1) << 4U;
                    position += (x & 0x3) << 2U;
                    std::memcpy(address, bufferData + position, sizeof(u32) * 4);
                }
            }
            ANativeWindow_unlockAndPost(window);
            bufferQueue.FreeBuffer(buffer->slot);
            vsyncEvent->Signal();
            if (prevTime != 0) {
                auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                state.logger->Error("{} ms, {} FPS", (now - prevTime) / 1000, 1000000 / (now - prevTime));
            }
            prevTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    }

    u32 GPU::OpenDevice(const std::string &path) {
        state.logger->Debug("Opening NVDRV device ({}): {}", fdIndex, path);
        auto type = device::nvDeviceMap.at(path);
        for (const auto &device : fdMap) {
            if (device.second->deviceType == type) {
                device.second->refCount++;
                fdMap[fdIndex] = device.second;
                return fdIndex++;
            }
        }
        std::shared_ptr<device::NvDevice> object;
        switch (type) {
            case (device::NvDeviceType::nvhost_ctrl):
                object = std::make_shared<device::NvHostCtrl>(state);
                break;
            case (device::NvDeviceType::nvhost_gpu):
            case (device::NvDeviceType::nvhost_vic):
            case (device::NvDeviceType::nvhost_nvdec):
                object = std::make_shared<device::NvHostChannel>(state, type);
                break;
            case (device::NvDeviceType::nvhost_ctrl_gpu):
                object = std::make_shared<device::NvHostCtrlGpu>(state);
                break;
            case (device::NvDeviceType::nvmap):
                object = std::make_shared<device::NvMap>(state);
                break;
            case (device::NvDeviceType::nvhost_as_gpu):
                object = std::make_shared<device::NvHostAsGpu>(state);
                break;
            default:
                throw exception("Cannot find NVDRV device");
        }
        deviceMap[type] = object;
        fdMap[fdIndex] = object;
        return fdIndex++;
    }

    void GPU::CloseDevice(u32 fd) {
        state.logger->Debug("Closing NVDRV device ({})", fd);
        try {
            auto device = fdMap.at(fd);
            if (!--device->refCount)
                deviceMap.erase(device->deviceType);
            fdMap.erase(fd);
        } catch (const std::out_of_range &) {
            state.logger->Warn("Trying to close non-existent FD");
        }
    }

    void GPU::Ioctl(u32 fd, u32 cmd, kernel::ipc::IpcRequest &request, kernel::ipc::IpcResponse &response) {
        state.logger->Debug("IOCTL on device: 0x{:X}, cmd: 0x{:X}", fd, cmd);
        try {
            if (!request.vecBufA.empty() && !request.vecBufB.empty()) {
                device::IoctlBuffers input(device::InputBuffer(request.vecBufA[0]), device::OutputBuffer(request.vecBufB[0]));
                fdMap.at(fd)->HandleIoctl(cmd, input);
                response.WriteValue<u32>(input.status);
            } else if (!request.vecBufX.empty() && !request.vecBufC.empty()) {
                device::IoctlBuffers input(device::InputBuffer(request.vecBufX[0]), device::OutputBuffer(request.vecBufC[0]));
                fdMap.at(fd)->HandleIoctl(cmd, input);
                response.WriteValue<u32>(input.status);
            } else if (!request.vecBufX.empty()) {
                device::IoctlBuffers input(device::InputBuffer(request.vecBufX[0]));
                fdMap.at(fd)->HandleIoctl(cmd, input);
                response.WriteValue<u32>(input.status);
            } else if (!request.vecBufC.empty()) {
                device::IoctlBuffers input(device::OutputBuffer(request.vecBufC[0]));
                fdMap.at(fd)->HandleIoctl(cmd, input);
                response.WriteValue<u32>(input.status);
            } else
                throw exception("Unknown IOCTL buffer configuration");
        } catch (const std::out_of_range &) {
            throw exception("IOCTL was requested on an invalid file descriptor");
        }
    }

    void GPU::SetDisplay(const std::string &name) {
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

    void GPU::CloseDisplay() {
        if (displayId == DisplayId::Null)
            state.logger->Warn("Trying to close uninitiated display");
        displayId = DisplayId::Null;
    }
}
