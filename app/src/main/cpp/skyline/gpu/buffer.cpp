// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <kernel/memory.h>
#include <kernel/types/KProcess.h>
#include <common/trace.h>
#include "buffer.h"

namespace skyline::gpu {
    void Buffer::SetupGuestMappings() {
        u8 *alignedData{util::AlignDown(guest->data(), PAGE_SIZE)};
        size_t alignedSize{static_cast<size_t>(util::AlignUp(guest->data() + guest->size(), PAGE_SIZE) - alignedData)};

        alignedMirror = gpu.state.process->memory.CreateMirror(alignedData, alignedSize);
        mirror = alignedMirror.subspan(static_cast<size_t>(guest->data() - alignedData), guest->size());

        trapHandle = gpu.state.nce->TrapRegions(*guest, true, [this] {
            std::lock_guard lock(*this);
            SynchronizeGuest(true); // We can skip trapping since the caller will do it
            WaitOnFence();
        }, [this] {
            std::lock_guard lock(*this);
            SynchronizeGuest(true);
            dirtyState = DirtyState::CpuDirty; // We need to assume the buffer is dirty since we don't know what the guest is writing
            WaitOnFence();
        });
    }

    Buffer::Buffer(GPU &gpu, GuestBuffer guest) : gpu(gpu), backing(gpu.memory.AllocateBuffer(guest.size())), guest(guest) {
        SetupGuestMappings();
    }

    Buffer::Buffer(GPU &gpu, vk::DeviceSize size) : gpu(gpu), backing(gpu.memory.AllocateBuffer(size)) {
        dirtyState = DirtyState::Clean; // Since this is a host-only buffer it's always going to be clean
    }

    Buffer::~Buffer() {
        std::lock_guard lock(*this);
        if (trapHandle)
            gpu.state.nce->DeleteTrap(*trapHandle);
        SynchronizeGuest(true);
        if (alignedMirror.valid())
            munmap(alignedMirror.data(), alignedMirror.size());
    }

    void Buffer::MarkGpuDirty() {
        if (dirtyState == DirtyState::GpuDirty || externallySynchronized || !guest) {
            externallySynchronized = false; // We want to handle synchronize internally after the GPU work is done
            return;
        }
        gpu.state.nce->RetrapRegions(*trapHandle, false);
        dirtyState = DirtyState::GpuDirty;
    }

    void Buffer::MarkExternallySynchronized() {
        TRACE_EVENT("gpu", "Buffer::MarkExternallySynchronized");
        if (externallySynchronized)
            return;

        if (dirtyState == DirtyState::GpuDirty)
            std::memcpy(mirror.data(), backing.data(), mirror.size());
        else if (dirtyState == DirtyState::CpuDirty)
            std::memcpy(backing.data(), mirror.data(), mirror.size());

        dirtyState = DirtyState::GpuDirty; // Any synchronization will take place on the GPU which in itself would make the buffer dirty
        gpu.state.nce->RetrapRegions(*trapHandle, false);
        externallySynchronized = true;
    }

    void Buffer::WaitOnFence() {
        TRACE_EVENT("gpu", "Buffer::WaitOnFence");

        auto lCycle{cycle.lock()};
        if (lCycle) {
            lCycle->Wait();
            cycle.reset();
        }
    }

    void Buffer::SynchronizeHost(bool rwTrap) {
        if (dirtyState != DirtyState::CpuDirty || !guest)
            return; // If the buffer has not been modified on the CPU or there's no guest buffer, there is no need to synchronize it

        WaitOnFence();

        if (externallySynchronized)
            return; // If the buffer is externally synchronized, we don't need to synchronize it

        TRACE_EVENT("gpu", "Buffer::SynchronizeHost");

        std::memcpy(backing.data(), mirror.data(), mirror.size());

        if (rwTrap) {
            gpu.state.nce->RetrapRegions(*trapHandle, false);
            dirtyState = DirtyState::GpuDirty;
        } else {
            gpu.state.nce->RetrapRegions(*trapHandle, true);
            dirtyState = DirtyState::Clean;
        }
    }

    void Buffer::SynchronizeHostWithCycle(const std::shared_ptr<FenceCycle> &pCycle, bool rwTrap) {
        if (dirtyState != DirtyState::CpuDirty || !guest || externallySynchronized)
            return;

        if (!cycle.owner_before(pCycle))
            WaitOnFence();

        if (externallySynchronized)
            return;

        TRACE_EVENT("gpu", "Buffer::SynchronizeHostWithCycle");

        std::memcpy(backing.data(), mirror.data(), mirror.size());

        if (rwTrap) {
            gpu.state.nce->RetrapRegions(*trapHandle, false);
            dirtyState = DirtyState::GpuDirty;
        } else {
            gpu.state.nce->RetrapRegions(*trapHandle, true);
            dirtyState = DirtyState::Clean;
        }
    }

    void Buffer::SynchronizeGuest(bool skipTrap, bool skipFence) {
        if (dirtyState != DirtyState::GpuDirty || !guest || externallySynchronized)
            return; // If the buffer has not been used on the GPU or there's no guest buffer, there is no need to synchronize it

        if (!skipFence)
            WaitOnFence();

        if (externallySynchronized)
            return; // If the buffer is externally synchronized, we don't need to synchronize it

        TRACE_EVENT("gpu", "Buffer::SynchronizeGuest");

        std::memcpy(mirror.data(), backing.data(), mirror.size());

        if (!skipTrap)
            gpu.state.nce->RetrapRegions(*trapHandle, true);
        dirtyState = DirtyState::Clean;
    }

    /**
     * @brief A FenceCycleDependency that synchronizes the contents of a host buffer with the guest buffer
     */
    struct BufferGuestSync : public FenceCycleDependency {
        std::shared_ptr<Buffer> buffer;

        explicit BufferGuestSync(std::shared_ptr<Buffer> buffer) : buffer(std::move(buffer)) {}

        ~BufferGuestSync() {
            TRACE_EVENT("gpu", "Buffer::BufferGuestSync");
            buffer->SynchronizeGuest();
        }
    };

    void Buffer::SynchronizeGuestWithCycle(const std::shared_ptr<FenceCycle> &pCycle) {
        if (!guest)
            return; // If there's no guest buffer, there is no need to synchronize it

        if (!cycle.owner_before(pCycle))
            WaitOnFence();

        pCycle->AttachObject(std::make_shared<BufferGuestSync>(shared_from_this()));
        cycle = pCycle;
    }

    void Buffer::Read(span<u8> data, vk::DeviceSize offset) {
        if (externallySynchronized || dirtyState == DirtyState::CpuDirty || dirtyState == DirtyState::Clean)
            std::memcpy(data.data(), mirror.data() + offset, data.size());
        else if (dirtyState == DirtyState::GpuDirty)
            std::memcpy(data.data(), backing.data() + offset, data.size());
    }

    void Buffer::Write(span<u8> data, vk::DeviceSize offset) {
        if (externallySynchronized || dirtyState == DirtyState::CpuDirty || dirtyState == DirtyState::Clean)
            std::memcpy(mirror.data() + offset, data.data(), data.size());
        if (!externallySynchronized && ((dirtyState == DirtyState::Clean) || dirtyState == DirtyState::GpuDirty))
            std::memcpy(backing.data() + offset, data.data(), data.size());
    }

    Buffer::BufferViewStorage::BufferViewStorage(vk::DeviceSize offset, vk::DeviceSize size, vk::Format format) : offset(offset), size(size), format(format) {}

    Buffer::BufferDelegate::BufferDelegate(std::shared_ptr<Buffer> pBuffer, Buffer::BufferViewStorage *view) : buffer(std::move(pBuffer)), view(view) {
        iterator = buffer->delegates.emplace(buffer->delegates.end(), this);
    }

    Buffer::BufferDelegate::~BufferDelegate() {
        std::scoped_lock lock(*this);
        buffer->delegates.erase(iterator);
    }

    void Buffer::BufferDelegate::lock() {
        auto lBuffer{std::atomic_load(&buffer)};
        while (true) {
            lBuffer->lock();

            auto latestBacking{std::atomic_load(&buffer)};
            if (lBuffer == latestBacking)
                return;

            lBuffer->unlock();
            lBuffer = latestBacking;
        }
    }

    void Buffer::BufferDelegate::unlock() {
        buffer->unlock();
    }

    bool Buffer::BufferDelegate::try_lock() {
        auto lBuffer{std::atomic_load(&buffer)};
        while (true) {
            bool success{lBuffer->try_lock()};

            auto latestBuffer{std::atomic_load(&buffer)};
            if (lBuffer == latestBuffer)
                // We want to ensure that the try_lock() was on the latest backing and not on an outdated one
                return success;

            if (success)
                // We only unlock() if the try_lock() was successful and we acquired the mutex
                lBuffer->unlock();
            lBuffer = latestBuffer;
        }
    }

    BufferView Buffer::GetView(vk::DeviceSize offset, vk::DeviceSize size, vk::Format format) {
        for (auto &view : views)
            if (view.offset == offset && view.size == size && view.format == format)
                return BufferView{shared_from_this(), &view};

        views.emplace_back(offset, size, format);
        return BufferView{shared_from_this(), &views.back()};
    }

    BufferView::BufferView(std::shared_ptr<Buffer> buffer, Buffer::BufferViewStorage *view) : bufferDelegate(std::make_shared<Buffer::BufferDelegate>(std::move(buffer), view)) {}

    void BufferView::AttachCycle(const std::shared_ptr<FenceCycle> &cycle) {
        auto buffer{bufferDelegate->buffer.get()};
        if (!buffer->cycle.owner_before(cycle)) {
            buffer->WaitOnFence();
            buffer->cycle = cycle;
            cycle->AttachObject(bufferDelegate);
        }
    }

    void BufferView::RegisterUsage(const std::function<void(const Buffer::BufferViewStorage &, const std::shared_ptr<Buffer> &)> &usageCallback) {
        usageCallback(*bufferDelegate->view, bufferDelegate->buffer);
        if (!bufferDelegate->usageCallback) {
            bufferDelegate->usageCallback = usageCallback;
        } else {
            bufferDelegate->usageCallback = [usageCallback, oldCallback = std::move(bufferDelegate->usageCallback)](const Buffer::BufferViewStorage &pView, const std::shared_ptr<Buffer> &buffer) {
                oldCallback(pView, buffer);
                usageCallback(pView, buffer);
            };
        }
    }

    void BufferView::Read(span<u8> data, vk::DeviceSize offset) const {
        bufferDelegate->buffer->Read(data, offset + bufferDelegate->view->offset);
    }

    void BufferView::Write(span<u8> data, vk::DeviceSize offset) const {
        bufferDelegate->buffer->Write(data, offset + bufferDelegate->view->offset);
    }
}
