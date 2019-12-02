#include "gpu.h"
#include "gpu/devices/nvhost_ctrl.h"
#include "gpu/devices/nvhost_ctrl_gpu.h"
#include "gpu/devices/nvhost_channel.h"
#include "gpu/devices/nvhost_as_gpu.h"
#include <kernel/types/KProcess.h>
#include <android/native_window_jni.h>

extern bool Halt;

namespace skyline::gpu {
    GPU::GPU(const DeviceState &state) : state(state), window(ANativeWindow_fromSurface(state.jvmManager->env, state.jvmManager->GetField("surface", "Landroid/view/Surface;"))), bufferQueue(state), vsyncEvent(std::make_shared<kernel::type::KEvent>(state)), bufferEvent(std::make_shared<kernel::type::KEvent>(state)) {
        ANativeWindow_acquire(window);
        resolution.width = static_cast<u32>(ANativeWindow_getWidth(window));
        resolution.height = static_cast<u32>(ANativeWindow_getHeight(window));
        format = ANativeWindow_getFormat(window);
    }

    GPU::~GPU() {
        ANativeWindow_release(window);
    }

    void GPU::Loop() {
        if(state.jvmManager->CheckNull("surface", "Landroid/view/Surface;")) {
            while (state.jvmManager->CheckNull("surface", "Landroid/view/Surface;")) {
                if(state.jvmManager->GetField<jboolean>("halt"))
                    return;
                sched_yield();
            }
            window = ANativeWindow_fromSurface(state.jvmManager->env, state.jvmManager->GetField("surface", "Landroid/view/Surface;"));
            ANativeWindow_acquire(window);
            resolution.width = static_cast<u32>(ANativeWindow_getWidth(window));
            resolution.height = static_cast<u32>(ANativeWindow_getHeight(window));
            format = ANativeWindow_getFormat(window);
        }
        if (!bufferQueue.displayQueue.empty()) {
            auto &buffer = bufferQueue.displayQueue.front();
            bufferQueue.displayQueue.pop();
            if (resolution != buffer->resolution || buffer->gbpBuffer.format != format) {
                ANativeWindow_setBuffersGeometry(window, buffer->resolution.width, buffer->resolution.height, buffer->gbpBuffer.format);
                resolution = buffer->resolution;
                format = buffer->gbpBuffer.format;
            }
            buffer->UpdateBuffer();
            u8 *inBuffer = buffer->dataBuffer.data();
            madvise(inBuffer, buffer->gbpBuffer.size, MADV_SEQUENTIAL);
            ANativeWindow_Buffer windowBuffer;
            ARect rect;
            ANativeWindow_lock(window, &windowBuffer, &rect);
            u8 *outBuffer = reinterpret_cast<u8 *>(windowBuffer.bits);
            const u32 strideBytes = buffer->gbpBuffer.stride * buffer->bpp;
            const u32 blockHeight = 1U << buffer->gbpBuffer.blockHeightLog2;
            const u32 blockHeightPixels = 8U << buffer->gbpBuffer.blockHeightLog2;
            const u32 widthBlocks = strideBytes >> 6U;
            const u32 heightBlocks = ((resolution.height) + blockHeightPixels - 1) >> (3 + buffer->gbpBuffer.blockHeightLog2);
            for (u32 blockY = 0; blockY < heightBlocks; blockY++) {
                for (u32 blockX = 0; blockX < widthBlocks; blockX++) {
                    for (u32 gobY = 0; gobY < blockHeight; gobY++) {
                        const u32 x = blockX * constant::GobStride;
                        const u32 y = blockY * blockHeightPixels + gobY * constant::GobHeight;
                        if (y < resolution.height) {
                            u8 *inBlock = inBuffer;
                            u8 *outBlock = outBuffer + (y * strideBytes) + x;
                            for (u32 i = 0; i < 32; i++) {
                                const u32 yT = ((i >> 1) & 0x06) | (i & 0x01); // NOLINT(hicpp-signed-bitwise)
                                const u32 xT = ((i << 3) & 0x10) | ((i << 1) & 0x20); // NOLINT(hicpp-signed-bitwise)
                                std::memcpy(outBlock + (yT * strideBytes) + xT, inBlock, sizeof(u128));
                                inBlock += sizeof(u128);
                            }
                        }
                        inBuffer += constant::GobSize;
                    }
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
            if(request.inputBuf.empty() || request.outputBuf.empty()) {
                if(request.inputBuf.empty()) {
                    device::IoctlData data(request.outputBuf.at(0));
                    fdMap.at(fd)->HandleIoctl(cmd, data);
                    response.Push<u32>(data.status);
                } else {
                    device::IoctlData data(request.inputBuf.at(0));
                    fdMap.at(fd)->HandleIoctl(cmd, data);
                    response.Push<u32>(data.status);
                }
            } else {
                device::IoctlData data(request.inputBuf.at(0), request.outputBuf.at(0));
                fdMap.at(fd)->HandleIoctl(cmd, data);
                response.Push<u32>(data.status);
            }
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
