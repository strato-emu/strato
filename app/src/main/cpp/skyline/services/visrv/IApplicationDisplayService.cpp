// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2019 Ryujinx Team and Contributors (https://github.com/Ryujinx/)

#include <gpu.h>
#include <kernel/types/KProcess.h>
#include <services/serviceman.h>
#include <services/hosbinder/IHOSBinderDriver.h>
#include "IApplicationDisplayService.h"
#include "ISystemDisplayService.h"
#include "IManagerDisplayService.h"
#include "results.h"

namespace skyline::service::visrv {
    IApplicationDisplayService::IApplicationDisplayService(const DeviceState &state, ServiceManager &manager, PrivilegeLevel level) : level(level), IDisplayService(state, manager) {}

    Result IApplicationDisplayService::GetRelayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(hosbinder, session, response);
        return {};
    }

    Result IApplicationDisplayService::GetIndirectDisplayTransactionService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (level < PrivilegeLevel::System)
            return result::IllegalOperation;
        manager.RegisterService(hosbinder, session, response);
        return {};
    }

    Result IApplicationDisplayService::GetSystemDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (level < PrivilegeLevel::System)
            return result::IllegalOperation;
        manager.RegisterService(SRVREG(ISystemDisplayService), session, response);
        return {};
    }

    Result IApplicationDisplayService::GetManagerDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (level < PrivilegeLevel::Manager)
            return result::IllegalOperation;
        manager.RegisterService(SRVREG(IManagerDisplayService), session, response);
        return {};
    }

    Result IApplicationDisplayService::ListDisplays(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct DisplayInfo {
            std::array<u8, 0x40> displayName{"Default"};
            u8 hasLimitedLayers{1};
            u8 pad[7];
            u64 maxLayers{1};
            u64 width{1920};
            u64 height{1080};
        } displayInfo;

        request.outputBuf.at(0).as<DisplayInfo>() = displayInfo;
        response.Push<u64>(1);
        return {};
    }

    Result IApplicationDisplayService::OpenDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto displayName(request.PopString());
        Logger::Debug("Opening display: {}", displayName);
        response.Push(hosbinder->OpenDisplay(displayName));
        return {};
    }

    Result IApplicationDisplayService::CloseDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto displayId{request.Pop<hosbinder::DisplayId>()};
        Logger::Debug("Closing display: {}", hosbinder::ToString(displayId));
        hosbinder->CloseDisplay(displayId);
        return {};
    }

    Result IApplicationDisplayService::OpenLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto displayName{request.PopString(0x40)};
        auto layerId{request.Pop<u64>()};
        Logger::Debug("Opening layer #{} on display: {}", layerId, displayName);

        auto displayId{hosbinder->OpenDisplay(displayName)};
        auto parcel{hosbinder->OpenLayer(displayId, layerId)};
        response.Push<u64>(parcel.WriteParcel(request.outputBuf.at(0)));

        return {};
    }

    Result IApplicationDisplayService::CloseLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u64 layerId{request.Pop<u64>()};
        Logger::Debug("Closing layer #{}", layerId);
        hosbinder->CloseLayer(layerId);
        return {};
    }

    Result IApplicationDisplayService::SetLayerScalingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto scalingMode{request.Pop<u64>()};
        auto layerId{request.Pop<u64>()};
        Logger::Debug("Setting Layer Scaling mode to '{}' for layer {}", scalingMode, layerId);
        return {};
    }

    Result IApplicationDisplayService::GetDisplayVsyncEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        KHandle handle{state.process->InsertItem(state.gpu->presentation.vsyncEvent)};
        Logger::Debug("V-Sync Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IApplicationDisplayService::ConvertScalingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 inScalingMode{request.Pop<u32>()};

        std::array<ScalingMode, 5> scalingModeLut{
            ScalingMode::None,
            ScalingMode::Freeze,
            ScalingMode::ScaleToLayer,
            ScalingMode::ScaleAndCrop,
            ScalingMode::PreserveAspectRatio,
        };

        if (scalingModeLut.size() <= inScalingMode)
            return result::InvalidArgument;

        auto scalingMode{scalingModeLut[inScalingMode]};
        if (scalingMode != ScalingMode::ScaleToLayer && scalingMode != ScalingMode::PreserveAspectRatio)
            return result::IllegalOperation;

        response.Push(scalingMode);

        return {};
    }

    Result IApplicationDisplayService::GetIndirectLayerImageMap(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto width{request.Pop<i64>()};
        auto height{request.Pop<i64>()};

        if (!request.outputBuf.empty()) {
            // As we don't support indirect layers, we just fill the output buffer with red
            auto imageBuffer{request.outputBuf.at(0)};
            std::fill(imageBuffer.begin(), imageBuffer.end(), 0xFF0000FF);
        }

        response.Push<i64>(width);
        response.Push<i64>(height);

        return {};
    }

    Result IApplicationDisplayService::GetIndirectLayerImageRequiredMemoryInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        i64 width{request.Pop<i64>()}, height{request.Pop<i64>()};

        if (width <= 0 || height <= 0)
            return result::InvalidDimensions;

        constexpr ssize_t A8B8G8R8Size{4}; //!< The size of a pixel in the A8B8G8R8 format, this format is used by indirect layers
        i64 layerSize{width * height * A8B8G8R8Size};

        constexpr ssize_t BlockSize{0x20000}; //!< The size of an arbitrarily defined block, the layer size must be aligned to a block
        response.Push<i64>(util::AlignUpNpot<i64>(layerSize, BlockSize));

        constexpr size_t DefaultAlignment{0x1000}; //!< The default alignment of the buffer
        response.Push<u64>(DefaultAlignment);

        return Result{};
    }
}
