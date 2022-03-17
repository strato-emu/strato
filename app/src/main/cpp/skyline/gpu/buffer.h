// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <nce.h>
#include "memory_manager.h"

namespace skyline::gpu {
    /**
     * @brief A descriptor for a GPU buffer on the guest
     */
    struct GuestBuffer {
        using Mappings = boost::container::small_vector<span < u8>, 3>;
        Mappings mappings; //!< Spans to CPU memory for the underlying data backing this buffer

        /**
         * @return The total size of the buffer by adding up the size of all mappings
         */
        vk::DeviceSize BufferSize() const;
    };

    struct BufferView;
    class BufferManager;

    /**
     * @brief A buffer which is backed by host constructs while being synchronized with the underlying guest buffer
     * @note This class conforms to the Lockable and BasicLockable C++ named requirements
     */
    class Buffer : public std::enable_shared_from_this<Buffer>, public FenceCycleDependency {
      private:
        GPU &gpu;
        std::mutex mutex; //!< Synchronizes any mutations to the buffer or its backing
        vk::DeviceSize size;
        memory::Buffer backing;
        GuestBuffer guest;

        span<u8> mirror{}; //!< A contiguous mirror of all the guest mappings to allow linear access on the CPU
        span<u8> alignedMirror{}; //!< The mirror mapping aligned to page size to reflect the full mapping
        std::optional<nce::NCE::TrapHandle> trapHandle{}; //!< The handle of the traps for the guest mappings
        enum class DirtyState {
            Clean, //!< The CPU mappings are in sync with the GPU buffer
            CpuDirty, //!< The CPU mappings have been modified but the GPU buffer is not up to date
            GpuDirty, //!< The GPU buffer has been modified but the CPU mappings have not been updated
        } dirtyState{DirtyState::CpuDirty}; //!< The state of the CPU mappings with respect to the GPU buffer

        /**
         * @brief Storage for all metadata about a specific view into the buffer, used to prevent redundant view creation and duplication of VkBufferView(s)
         */
        struct BufferViewStorage {
          public:
            vk::DeviceSize offset;
            vk::DeviceSize range;
            vk::Format format;

            BufferViewStorage(vk::DeviceSize offset, vk::DeviceSize range, vk::Format format);
        };
        std::list<BufferViewStorage> views; //!< BufferViewStorage(s) that are backed by this Buffer, used for storage and repointing to a new Buffer on deletion

        friend BufferView;
        friend BufferManager;

        /**
         * @brief Sets up mirror mappings for the guest mappings
         */
        void SetupGuestMappings();

      public:
        std::weak_ptr<FenceCycle> cycle; //!< A fence cycle for when any host operation mutating the buffer has completed, it must be waited on prior to any mutations to the backing

        constexpr vk::Buffer GetBacking() {
            return backing.vkBuffer;
        }

        Buffer(GPU &gpu, GuestBuffer guest);

        ~Buffer();

        /**
         * @brief Acquires an exclusive lock on the buffer for the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void lock() {
            mutex.lock();
        }

        /**
         * @brief Relinquishes an existing lock on the buffer by the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void unlock() {
            mutex.unlock();
        }

        /**
         * @brief Attempts to acquire an exclusive lock but returns immediately if it's captured by another thread
         * @note Naming is in accordance to the Lockable named requirement
         */
        bool try_lock() {
            return mutex.try_lock();
        }

        /**
         * @brief Marks the buffer as dirty on the GPU, it will be synced on the next call to SynchronizeGuest
         * @note This **must** be called after syncing the buffer to the GPU not before
         * @note The buffer **must** be locked prior to calling this
         */
        void MarkGpuDirty();

        /**
         * @brief Waits on a fence cycle if it exists till it's signalled and resets it after
         * @note The buffer **must** be locked prior to calling this
         */
        void WaitOnFence();

        /**
         * @brief Synchronizes the host buffer with the guest
         * @param rwTrap If true, the guest buffer will be read/write trapped rather than only being write trapped which is more efficient than calling MarkGpuDirty directly after
         * @note The buffer **must** be locked prior to calling this
         */
        void SynchronizeHost(bool rwTrap = false);

        /**
         * @brief Synchronizes the host buffer with the guest
         * @param cycle A FenceCycle that is checked against the held one to skip waiting on it when equal
         * @param rwTrap If true, the guest buffer will be read/write trapped rather than only being write trapped which is more efficient than calling MarkGpuDirty directly after
         * @note The buffer **must** be locked prior to calling this
         */
        void SynchronizeHostWithCycle(const std::shared_ptr<FenceCycle> &cycle, bool rwTrap = false);

        /**
         * @brief Synchronizes the guest buffer with the host buffer
         * @param skipTrap If true, setting up a CPU trap will be skipped and the dirty state will be Clean/CpuDirty
         * @note The buffer **must** be locked prior to calling this
         */
        void SynchronizeGuest(bool skipTrap = false);

        /**
         * @brief Synchronizes the guest buffer with the host buffer when the FenceCycle is signalled
         * @note The buffer **must** be locked prior to calling this
         * @note The guest buffer should not be null prior to calling this
         */
        void SynchronizeGuestWithCycle(const std::shared_ptr<FenceCycle> &cycle);

        /**
         * @brief Writes data at the specified offset in the buffer
         */
        void Write(span<u8> data, vk::DeviceSize offset);

        /**
         * @return A cached or newly created view into this buffer with the supplied attributes
         * @note The buffer **must** be locked prior to calling this
         */
        BufferView GetView(vk::DeviceSize offset, vk::DeviceSize range, vk::Format format = {});
    };

    /**
     * @brief A contiguous view into a Vulkan Buffer that represents a single guest buffer (as opposed to Buffer objects which contain multiple)
     * @note The object **must** be locked prior to accessing any members as values will be mutated
     * @note This class conforms to the Lockable and BasicLockable C++ named requirements
     */
    struct BufferView {
        std::shared_ptr<Buffer> buffer;
        Buffer::BufferViewStorage *view;

        BufferView(std::shared_ptr<Buffer> buffer, Buffer::BufferViewStorage *view);

        constexpr BufferView(nullptr_t = nullptr) : buffer(nullptr), view(nullptr) {}

        constexpr operator bool() const {
            return view != nullptr;
        }

        constexpr Buffer::BufferViewStorage *operator->() {
            return view;
        }

        operator std::shared_ptr<FenceCycleDependency>() {
            return buffer;
        }

        /**
         * @brief Acquires an exclusive lock on the buffer for the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void lock();

        /**
         * @brief Relinquishes an existing lock on the buffer by the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void unlock();

        /**
         * @brief Attempts to acquire an exclusive lock but returns immediately if it's captured by another thread
         * @note Naming is in accordance to the Lockable named requirement
         */
        bool try_lock();
    };
}
