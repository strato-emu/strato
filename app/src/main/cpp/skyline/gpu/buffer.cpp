// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <common/trace.h>
#include "buffer.h"

namespace skyline::gpu {
    vk::DeviceSize GuestBuffer::BufferSize() const {
        vk::DeviceSize size{};
        for (const auto &buffer : mappings)
            size += buffer.size_bytes();
        return size;
    }

    Buffer::Buffer(GPU &gpu, GuestBuffer guest) : size(guest.BufferSize()), backing(gpu.memory.AllocateBuffer(size)), guest(std::move(guest)) {
        SynchronizeHost();
    }

    void Buffer::WaitOnFence() {
        TRACE_EVENT("gpu", "Buffer::WaitOnFence");

        auto lCycle{cycle.lock()};
        if (lCycle) {
            lCycle->Wait();
            cycle.reset();
        }
    }

    void Buffer::SynchronizeHost() {
        WaitOnFence();

        TRACE_EVENT("gpu", "Buffer::SynchronizeHost");

        auto host{backing.data()};
        for (auto &mapping : guest.mappings) {
            auto mappingSize{mapping.size_bytes()};
            std::memcpy(host, mapping.data(), mappingSize);
            host += mappingSize;
        }
    }

    void Buffer::SynchronizeHostWithCycle(const std::shared_ptr<FenceCycle> &pCycle) {
        if (pCycle != cycle.lock())
            WaitOnFence();

        TRACE_EVENT("gpu", "Buffer::SynchronizeHostWithCycle");

        auto host{backing.data()};
        for (auto &mapping : guest.mappings) {
            auto mappingSize{mapping.size_bytes()};
            std::memcpy(host, mapping.data(), mappingSize);
            host += mappingSize;
        }
    }

    void Buffer::SynchronizeGuest() {
        WaitOnFence();

        TRACE_EVENT("gpu", "Buffer::SynchronizeGuest");

        auto host{backing.data()};
        for (auto &mapping : guest.mappings) {
            auto mappingSize{mapping.size_bytes()};
            std::memcpy(mapping.data(), host, mappingSize);
            host += mappingSize;
        }
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
        if (pCycle != cycle.lock())
            WaitOnFence();

        pCycle->AttachObject(std::make_shared<BufferGuestSync>(shared_from_this()));
        cycle = pCycle;
    }

    std::shared_ptr<BufferView> Buffer::GetView(vk::DeviceSize offset, vk::DeviceSize range, vk::Format format) {
        for (const auto &viewWeak : views) {
            auto view{viewWeak.lock()};
            if (view && view->offset == offset && view->range == range && view->format == format)
                return view;
        }

        auto view{std::make_shared<BufferView>(shared_from_this(), offset, range, format)};
        views.push_back(view);
        return view;
    }

    BufferView::BufferView(std::shared_ptr<Buffer> backing, vk::DeviceSize offset, vk::DeviceSize range, vk::Format format) : buffer(std::move(backing)), offset(offset), range(range), format(format) {}

    void BufferView::lock() {
        auto currentBacking{std::atomic_load(&buffer)};
        while (true) {
            currentBacking->lock();

            auto newBacking{std::atomic_load(&buffer)};
            if (currentBacking == newBacking)
                return;

            currentBacking->unlock();
            currentBacking = newBacking;
        }
    }

    void BufferView::unlock() {
        buffer->unlock();
    }

    bool BufferView::try_lock() {
        auto currentBacking{std::atomic_load(&buffer)};
        while (true) {
            bool success{currentBacking->try_lock()};

            auto newBacking{std::atomic_load(&buffer)};
            if (currentBacking == newBacking)
                return success;

            if (success)
                currentBacking->unlock();
            currentBacking = newBacking;
        }
    }
}
