// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "base.h"
#include "exception.h"
#include "logger.h"
#include "span.h"

namespace skyline::dirty {
    template<size_t, size_t, size_t>
    class Manager;

    /**
     * @brief An opaque handle to a dirty subresource
     */
    struct Handle {
      private:
        template<size_t, size_t, size_t>
        friend class Manager;

        bool *dirtyPtr; //!< Underlying target ptr

      public:
        explicit Handle(bool *dirtyPtr) : dirtyPtr{dirtyPtr} {}
    };

    /**
     * @brief Implements a way to track dirty subresources within a region of memory
     * @tparam ManagedResourceSize Size of the managed resource in bytes
     * @tparam Granularity Minimum granularity of a subresource in bytes
     * @tparam OverlapPoolSize Size of the pool used to store handles when there are multiple bound to the same subresource
     * @note This class is *NOT* thread-safe
     */
    template<size_t ManagedResourceSize, size_t Granularity, size_t OverlapPoolSize = 0x2000>
    class Manager {
      private:
        struct BindingState {
            enum class Type : u32 {
                None, //!< No handles are bound
                Inline, //!< `inlineDirtyPtr` contains a pointer to the single bound handle for the entry
                OverlapSpan, //!< `overlapSpanDirtyPtrs` contains an array of pointers to handles bound to the entry
            } type{Type::None};
            u32 overlapSpanSize{}; //!< Size of the array that overlapSpanDirtyPtrs points to

            union {
                bool *inlineDirtyPtr;
                bool **overlapSpanDirtyPtrs;
            };

            /**
             * @return An array of pointers to handles bound to the entry
             */
            span<bool *> GetOverlapSpan() {
                return {overlapSpanDirtyPtrs, overlapSpanSize};
            }
        };

        std::array<bool *, OverlapPoolSize> overlapPool{}; //!< Backing pool for `overlapSpanDirtyPtrs` in entry
        bool **freeOverlapPtr{}; //!< Pointer to the next free entry in `overlapPool`
        
        std::array<BindingState, ManagedResourceSize / Granularity> states{}; //!< The dirty binding states for the entire managed resource

        uintptr_t managedResourceBaseAddress; //!< The base address of the managed resource

      public:
        template<typename ManagedResourceType> requires (std::is_standard_layout_v<ManagedResourceType> && sizeof(ManagedResourceType) == ManagedResourceSize)
        Manager(ManagedResourceType &managedResource) : managedResourceBaseAddress{reinterpret_cast<uintptr_t>(&managedResource)}, freeOverlapPtr{overlapPool.data()} {}

        /**
         * @brief Binds a handle to a subresource, automatically handling overlaps
         */
        void Bind(Handle handle, uintptr_t subresourceAddress, size_t subresourceSizeBytes) {
            if (managedResourceBaseAddress > subresourceAddress)
                throw exception("Dirty subresource address is below the managed resource base address");

            size_t subresourceAddressOffset{subresourceAddress - managedResourceBaseAddress};

            // Validate
            if (subresourceAddressOffset < 0 || (subresourceAddressOffset + subresourceSizeBytes) >= ManagedResourceSize)
                throw exception("Dirty subresource address is not within the managed resource");

            if (subresourceSizeBytes % Granularity)
                throw exception("Dirty subresource size isn't aligned to the tracking granularity");

            if (subresourceAddressOffset % Granularity)
                throw exception("Dirty subresource offset isn't aligned to the tracking granularity");

            // Insert
            size_t subresourceIndex{static_cast<size_t>(subresourceAddressOffset / Granularity)};
            size_t subresourceSize{subresourceSizeBytes / Granularity};

            for (size_t i{subresourceIndex}; i < subresourceIndex + subresourceSize; i++) {
                auto &state{states[i]};
                
                if (state.type == BindingState::Type::None) {
                    state.type = BindingState::Type::Inline;
                    state.inlineDirtyPtr = handle.dirtyPtr;
                } else if (state.type == BindingState::Type::Inline) {
                    state.type = BindingState::Type::OverlapSpan;

                    // Save the old inline dirty pointer since we'll need to insert it into the new overlap span and setting the overlap span ptr will overwrite it
                    bool *origDirtyPtr{state.inlineDirtyPtr};

                    // Point to a new pool allocation that can hold the overlap
                    state.overlapSpanDirtyPtrs = freeOverlapPtr;
                    state.overlapSpanSize = 2; // Existing inline handle + our new handle

                    // Check if the pool allocation is valid
                    if (freeOverlapPtr + state.overlapSpanSize >= overlapPool.end())
                        throw exception("Dirty overlap pool is full");

                    // Write overlap to our new pool allocation
                    *freeOverlapPtr++ = origDirtyPtr;
                    *freeOverlapPtr++ = handle.dirtyPtr;
                } else if (state.type == BindingState::Type::OverlapSpan) {
                    auto originalOverlapSpan{state.GetOverlapSpan()};

                    // Point to a new pool allocation that can hold all the old overlaps + our new overlap
                    state.overlapSpanSize++;
                    state.overlapSpanDirtyPtrs = freeOverlapPtr;

                    // Check if the pool allocation is valid
                    if (freeOverlapPtr + state.overlapSpanSize >= overlapPool.end())
                        throw exception("Dirty overlap pool is full");

                    // Write all overlaps to our new pool allocation
                    auto newOverlapSpan{state.GetOverlapSpan()};
                    newOverlapSpan.copy_from(originalOverlapSpan); // Copy old overlaps
                    *newOverlapSpan.last(1).data() = handle.dirtyPtr; // Write new overlap
                    freeOverlapPtr += state.overlapSpanSize;
                }
            }
        }

        template<typename SubresourceType> requires std::is_standard_layout_v<SubresourceType>
        void Bind(Handle handle, SubresourceType &subresource) {
            Bind(handle, reinterpret_cast<uintptr_t>(&subresource), sizeof(SubresourceType));
        }

        template<typename... Args>
        void Bind(Handle handle, Args && ...subresources) {
            (Bind(handle, subresources), ...);
        }

        /**
         * @brief Marks part of the managed resource as dirty
         * @note This *MUST NOT* be called after any bound handles have been destroyed
         */
        void MarkDirty(size_t index) {
            auto &state{states[index]};
            if (state.type == BindingState::Type::None) [[likely]] {
                return;
            } else if (state.type == BindingState::Type::Inline) {
                *state.inlineDirtyPtr = true;
            } else if (state.type == BindingState::Type::OverlapSpan) [[unlikely]] {
                for (auto &dirtyPtr : state.GetOverlapSpan())
                    *dirtyPtr = true;
            }
        }
    };

    /**
     * @brief Encapsulates a dirty subresource to allow for automatic binding on construction
     */
    template<typename T>
    class BoundSubresource {
      private:
        const T subresource;

      public:
        template<typename ManagerT>
        BoundSubresource(ManagerT &manager, dirty::Handle handle, const T &subresource) : subresource{subresource} {
            subresource.DirtyBind(manager, handle);
        }

        const T *operator->() const {
            return &subresource;
        }

        const T &operator*() const {
            return subresource;
        }
    };

    class ManualDirty {};

    /**
     * @brief ManualDirty but with a `Refresh()` method that should be called for every `Update()`, irrespective of dirty state
     */
    class RefreshableManualDirty : ManualDirty {};

    /**
     * @brief ManualDirty but with a `PurgeCaches()` method to purge caches that would usually be kept even after being marked dirty
     */
    class CachedManualDirty : ManualDirty {};

    /**
     * @brief Wrapper around an object that holds dirty state and provides convinient functionality for dirty tracking
     */
    template<typename T> requires (std::is_base_of_v<ManualDirty, T>)
    class ManualDirtyState {
      private:
        T value; //!< The underlying object
        bool dirty{true}; //!< Whether the value is dirty

        /**
         * @return An opaque handle that can be used to modify dirty state
         */
        Handle GetDirtyHandle() {
            return Handle{&dirty};
        }

      public:
        template<typename... Args>
        ManualDirtyState(Args &&... args) : value{GetDirtyHandle(), std::forward<Args>(args)...} {}

        /**
         * @brief Cleans the object of its dirty state and refreshes it if necessary
         * @note This *MUST* be called before any accesses to the underlying object without *ANY* calls to `MarkDirty()` in between
         */
        template<typename... Args>
        void Update(Args &&... args) {
            if (dirty) {
                dirty = false;
                value.Flush(std::forward<Args>(args)...);
            } else if constexpr (std::is_base_of_v<RefreshableManualDirty, T>) {
                if (value.Refresh(std::forward<Args>(args)...))
                    value.Flush(std::forward<Args>(args)...);
            }
        }

        /**
         * @brief Marks the object as dirty
         * @param purgeCaches Whether to purge caches of the object that would usually be kept even after being marked dirty
         */
        void MarkDirty(bool purgeCaches) {
            dirty = true;

            if constexpr (std::is_base_of_v<CachedManualDirty, T>)
                if (purgeCaches)
                    value.PurgeCaches();
        }


        /**
         * @brief Returns a reference to the underlying object
         * @note `Update()` *MUST* be called before calling this, without *ANY* calls to `MarkDirty()` in between
         */
        T &Get() {
            return value;
        }

        template<typename... Args>
        T &UpdateGet(Args &&... args) {
            Update(std::forward<Args>(args)...);
            return value;
        }
    };
}
