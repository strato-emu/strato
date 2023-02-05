// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <kernel/types/KProcess.h>
#include "IHOSBinderDriver.h"
#include "GraphicBufferProducer.h"

namespace skyline::service::hosbinder {
    IHOSBinderDriver::IHOSBinderDriver(const DeviceState &state, ServiceManager &manager, nvdrv::core::NvMap &nvMap) : BaseService(state, manager), nvMap(nvMap) {}

    Result IHOSBinderDriver::TransactParcel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // We opted for just supporting a single layer and display as it's what basically all games use and wasting cycles on it is pointless
        // If this was not done then we would need to maintain an array of GraphicBufferProducer objects for each layer and send the request for it specifically
        // There would also need to be an external compositor which composites all the graphics buffers submitted to every GraphicBufferProducer

        auto binderHandle{request.Pop<u32>()};
        if (binderHandle != DefaultBinderLayerHandle)
            throw exception("Transaction on unknown binder object: #{}", binderHandle);

        auto code{request.Pop<GraphicBufferProducer::TransactionCode>()};

        Parcel in(request.inputBuf.at(0), state, true);
        Parcel out(state);

        if (!layer)
            throw exception("Transacting parcel with non-existant layer");
        layer->OnTransact(code, in, out);

        out.WriteParcel(request.outputBuf.at(0));
        return {};
    }

    Result IHOSBinderDriver::AdjustRefcount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto binderHandle{request.Pop<u32>()};
        if (binderHandle != DefaultBinderLayerHandle)
            throw exception("Adjusting Binder object reference count for unknown object: #{}", binderHandle);

        auto value{request.Pop<i32>()};
        bool isStrong{request.Pop<u32>() != 0};
        if (isStrong) {
            if (layerStrongReferenceCount != InitialStrongReferenceCount)
                layerStrongReferenceCount += value;
            else
                layerStrongReferenceCount = value;

            if (layerStrongReferenceCount < 0) {
                Logger::Warn("Strong reference count is lower than 0: {} + {} = {}", (layerStrongReferenceCount - value), value, layerStrongReferenceCount);
                layerStrongReferenceCount = 0;
            }

            if (layerStrongReferenceCount == 0)
                layer.reset();
        } else {
            layerWeakReferenceCount += value;

            if (layerWeakReferenceCount < 0) {
                Logger::Warn("Weak reference count is lower than 0: {} + {} = {}", (layerWeakReferenceCount - value), value, layerWeakReferenceCount);
                layerWeakReferenceCount = 0;
            }

            if (layerWeakReferenceCount == 0 && layerStrongReferenceCount < 1)
                layer.reset();
        }

        Logger::Debug("Reference Change: {} {} reference (S{} W{})", value, isStrong ? "strong" : "weak", layerStrongReferenceCount, layerWeakReferenceCount);

        return {};
    }

    Result IHOSBinderDriver::GetNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto binderHandle{request.Pop<u32>()};
        if (binderHandle != DefaultBinderLayerHandle)
            throw exception("Getting handle from unknown binder object: #{}", binderHandle);

        constexpr u32 BufferEventHandleId{0xF}; //!< The ID of the buffer event handle in the layer object
        auto handleId{request.Pop<u32>()};
        if (handleId != BufferEventHandleId)
            throw exception("Getting unknown handle from binder object: 0x{:X}", handleId);

        KHandle handle{state.process->InsertItem(layer->bufferEvent)};
        Logger::Debug("Display Buffer Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);

        return {};
    }

    DisplayId IHOSBinderDriver::OpenDisplay(std::string_view name) {
        if (name.length() > sizeof(u64))
            throw exception("Opening display with name larger than sizeof(u64): '{}' ({})", name, name.length());

        auto newDisplayId{[&]() -> DisplayId {
            #define DISPLAY_CASE(id) case util::MakeMagic<u64>(#id): { return DisplayId::id; }
            switch (util::MakeMagic<u64>(name)) {
                DISPLAY_CASE(Default)
                DISPLAY_CASE(External)
                DISPLAY_CASE(Edid)
                DISPLAY_CASE(Internal)
                DISPLAY_CASE(Null)
                default:
                    throw exception("Opening non-existent display: '{}'", name);
            }
            #undef DISPLAY_CASE
        }()};
        if (displayId != DisplayId::Null && displayId != newDisplayId)
            throw exception("Opening a new display ({}) prior to closing opened display ({})", name, ToString(displayId));

        return displayId = newDisplayId;
    }

    void IHOSBinderDriver::CloseDisplay(DisplayId id) {
        if (displayId != id)
            throw exception("Closing an unopened display: {} (Currently open display: {})", ToString(id), ToString(displayId));
        displayId = DisplayId::Null;
    }

    u64 IHOSBinderDriver::CreateLayer(DisplayId pDisplayId) {
        if (pDisplayId != displayId)
            throw exception("Creating layer on unopened display: '{}'", ToString(pDisplayId));
        else if (!layer) {
            layerStrongReferenceCount = InitialStrongReferenceCount;
            layerWeakReferenceCount = 0;
            layer = std::make_shared<GraphicBufferProducer>(state, nvMap);
        }
        else // Ignore new layer creations if one already exists
            Logger::Warn("Creation of multiple layers is not supported. Ignoring creation of new layers.");

        return DefaultLayerId;
    }

    Parcel IHOSBinderDriver::OpenLayer(DisplayId pDisplayId, u64 layerId) {
        if (pDisplayId != displayId)
            throw exception("Opening layer #{} with unopened display: '{}'", layerId, ToString(pDisplayId));
        else if (layerId != DefaultLayerId)
            throw exception("Attempting to open unrecognized layer #{}", layerId);
        else if (!layer)
            throw exception("Opening layer #{} prior to creation or after destruction", layerId);

        Parcel parcel(state);
        // Flat Binder with the layer's IGraphicBufferProducer
        // https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:bionic/libc/kernel/uapi/linux/binder.h;l=47-57
        parcel.Push<u32>(0x2); // Type of the IBinder
        parcel.Push<u32>(0); // Flags
        parcel.Push<u64>(DefaultBinderLayerHandle); // Handle
        parcel.Push<u64>(0); // Cookie
        // Unknown HOS-specific layer properties
        parcel.Push(util::MakeMagic<u64>("dispdrv\0"));
        parcel.Push<u64>({}); // Unknown

        parcel.PushObject(0); // Offset of flattened IBinder relative to Parcel data

        layerWeakReferenceCount++; // IBinder represents a weak reference to the layer

        return parcel;
    }

    void IHOSBinderDriver::CloseLayer(u64 layerId) {
        if (layerId != DefaultLayerId)
            throw exception("Closing non-existent layer #{}", layerId);
        else if (layerWeakReferenceCount == 0)
            throw exception("Closing layer #{} which has no weak references to it", layerId);

        if (--layerWeakReferenceCount == 0 && layerStrongReferenceCount < 1)
            layer.reset();
    }

    void IHOSBinderDriver::DestroyLayer(u64 layerId) {
        if (layerId != DefaultLayerId)
            throw exception("Destroying non-existent layer #{}", layerId);
        else if (layer)
            throw exception("Destroying layer #{} which hasn't been closed: Weak References: {}, Strong References: {}", layerWeakReferenceCount, layerStrongReferenceCount);
    }
}
