// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <nce.h>
#include "memory_manager.h"

namespace skyline::gpu {
    using GuestBuffer = span<u8>; //!< The CPU mapping for the guest buffer, multiple mappings for buffers aren't supported since overlaps cannot be reconciled

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
        memory::Buffer backing;
        std::optional<GuestBuffer> guest;

        span<u8> mirror{}; //!< A contiguous mirror of all the guest mappings to allow linear access on the CPU
        span<u8> alignedMirror{}; //!< The mirror mapping aligned to page size to reflect the full mapping
        std::optional<nce::NCE::TrapHandle> trapHandle{}; //!< The handle of the traps for the guest mappings
        enum class DirtyState {
            Clean, //!< The CPU mappings are in sync with the GPU buffer
            CpuDirty, //!< The CPU mappings have been modified but the GPU buffer is not up to date
            GpuDirty, //!< The GPU buffer has been modified but the CPU mappings have not been updated
        } dirtyState{DirtyState::CpuDirty}; //!< The state of the CPU mappings with respect to the GPU buffer
        bool externallySynchronized{}; //!< Whether the host buffer is externally synchronized with the guest buffer, disables the buffer synchronization and aims to retain guest/host buffer data across buffer recreation

      public:
        /**
         * @brief Storage for all metadata about a specific view into the buffer, used to prevent redundant view creation and duplication of VkBufferView(s)
         */
        struct BufferViewStorage {
            vk::DeviceSize offset;
            vk::DeviceSize size;
            vk::Format format;

            BufferViewStorage(vk::DeviceSize offset, vk::DeviceSize size, vk::Format format);
        };

      private:
        std::list<BufferViewStorage> views; //!< BufferViewStorage(s) that are backed by this Buffer, used for storage and repointing to a new Buffer on deletion

      public:
        /**
         * @brief A delegate for a strong reference to a Buffer by a BufferView which can be changed to another Buffer transparently
         * @note This class conforms to the Lockable and BasicLockable C++ named requirements
         */
        struct BufferDelegate : public FenceCycleDependency {
            std::shared_ptr<Buffer> buffer;
            Buffer::BufferViewStorage *view;
            std::function<void(const BufferViewStorage &, const std::shared_ptr<Buffer> &)> usageCallback;
            std::list<BufferDelegate *>::iterator iterator;

            BufferDelegate(std::shared_ptr<Buffer> buffer, Buffer::BufferViewStorage *view);

            ~BufferDelegate();

            void lock();

            void unlock();

            bool try_lock();
        };

      private:
        std::list<BufferDelegate *> delegates; //!< The reference delegates for this buffer, used to prevent the buffer from being deleted while it is still in use

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

        /**
         * @brief Creates a host-only Buffer which isn't backed by any guest buffer
         * @note The created buffer won't have a mirror so any operations cannot depend on a mirror existing
         */
        Buffer(GPU &gpu, vk::DeviceSize size);

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
         * @note This clears the externally synchronized flag automatically
         * @note This **must** be called after syncing the buffer to the GPU not before
         * @note The buffer **must** be locked prior to calling this
         */
        void MarkGpuDirty();

        /**
         * @brief Marks the buffer as externally synchronized and automatically synchronizes the host buffer and guest buffer, ensuring the buffer is GPU dirty by the end of the current cycle is the responsibility of the API user
         * @note The buffer **must** be locked and have the desired fence attached prior to calling this
         */
        void MarkExternallySynchronized();

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
         * @param skipFence If true, waiting on the currently attached fence will be skipped
         * @note The buffer **must** be locked prior to calling this
         */
        void SynchronizeGuest(bool skipTrap = false, bool skipFence = false);

        /**
         * @brief Synchronizes the guest buffer with the host buffer when the FenceCycle is signalled
         * @note The buffer **must** be locked prior to calling this
         * @note The guest buffer should not be null prior to calling this
         */
        void SynchronizeGuestWithCycle(const std::shared_ptr<FenceCycle> &cycle);

        /**
         * @brief Reads data at the specified offset in the buffer
         * @note The buffer **must** be locked prior to calling this
         * @note If this buffer is externally synchronized, this will read exclusively from the guest buffer
         */
        void Read(span<u8> data, vk::DeviceSize offset);

        /**
         * @brief Writes data at the specified offset in the buffer
         * @note The buffer **must** be locked prior to calling this
         * @note If this buffer is externally synchronized, this will write to the guest buffer and not to the host buffer
         */
        void Write(span<u8> data, vk::DeviceSize offset);

        /**
         * @return A cached or newly created view into this buffer with the supplied attributes
         * @note The buffer **must** be locked prior to calling this
         */
        BufferView GetView(vk::DeviceSize offset, vk::DeviceSize size, vk::Format format = {});
    };

    /**
     * @brief A contiguous view into a Vulkan Buffer that represents a single guest buffer (as opposed to Buffer objects which contain multiple)
     * @note The object **must** be locked prior to accessing any members as values will be mutated
     * @note This class conforms to the Lockable and BasicLockable C++ named requirements
     */
    struct BufferView {
        std::shared_ptr<Buffer::BufferDelegate> bufferDelegate;

        BufferView(std::shared_ptr<Buffer> buffer, Buffer::BufferViewStorage *view);

        constexpr BufferView(nullptr_t = nullptr) : bufferDelegate(nullptr) {}

        /**
         * @brief Acquires an exclusive lock on the buffer for the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void lock() const {
            bufferDelegate->lock();
        }

        /**
         * @brief Relinquishes an existing lock on the buffer by the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void unlock() const {
            bufferDelegate->unlock();
        }

        /**
         * @brief Attempts to acquire an exclusive lock but returns immediately if it's captured by another thread
         * @note Naming is in accordance to the Lockable named requirement
         */
        bool try_lock() const {
            return bufferDelegate->try_lock();
        }

        constexpr operator bool() const {
            return bufferDelegate != nullptr;
        }

        /**
         * @note The buffer **must** be locked prior to calling this
         */
        Buffer::BufferDelegate *operator->() const {
            return bufferDelegate.get();
        }

        /**
         * @brief Attaches a fence cycle to the underlying buffer in a way that it will be synchronized with the latest backing buffer
         * @note The view **must** be locked prior to calling this
         */
        void AttachCycle(const std::shared_ptr<FenceCycle> &cycle);

        /**
         * @brief Registers a callback for a usage of this view, it may be called multiple times due to the view being recreated with different backings
         * @note The callback will be automatically called the first time after registration
         * @note The view **must** be locked prior to calling this
         */
        void RegisterUsage(const std::function<void(const Buffer::BufferViewStorage &, const std::shared_ptr<Buffer> &)> &usageCallback);

        /**
         * @brief Reads data at the specified offset in the view
         * @note The view **must** be locked prior to calling this
         * @note If this buffer is externally synchronized, this will read exclusively from the guest buffer
         */
        void Read(span<u8> data, vk::DeviceSize offset) const;

        /**
         * @brief Writes data at the specified offset in the view
         * @note The view **must** be locked prior to calling this
         * @note If this buffer is externally synchronized, this will write to the guest buffer and not to the host buffer
         */
        void Write(span<u8> data, vk::DeviceSize offset) const;
    };
}
