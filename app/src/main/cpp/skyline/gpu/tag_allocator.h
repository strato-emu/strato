// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/base.h>

namespace skyline::gpu {
    struct ContextTag;

    static ContextTag AllocateTag();

    /**
     * @brief A unique tag associated with a single "context" an abstraction to allow concurrent locking of resources by different parts of a single context
     */
    struct ContextTag {
      private:
        size_t key;

        friend ContextTag AllocateTag();

        constexpr ContextTag(size_t key) : key{key} {}

      public:
        constexpr ContextTag() : key{} {}

        constexpr bool operator==(const ContextTag &other) const {
            return key == other.key;
        }

        constexpr bool operator!=(const ContextTag &other) const {
            return key != other.key;
        }

        constexpr operator bool() const {
            return key != 0;
        }
    };

    /**
     * @return A globally unique tag to utilize for any operations
     */
    inline ContextTag AllocateTag() {
        static std::atomic<size_t> key{1};
        return ContextTag{key++};
    }

    /**
     * @brief A scoped lock specially designed for classes with ContextTag-based locking
     * @note This will unlock the tag when the scope is exited, **if** it locked the tag in the first place and `Release` has not been called
     */
    template<typename T>
    class ContextLock {
      private:
        T *resource;
        bool ownsLock; //!< If when this ContextLocked initially locked `resource`, it had never been locked for this context before

      public:
        ContextLock(ContextTag tag, T &resource) : resource{&resource}, ownsLock{resource.LockWithTag(tag)} {}

        ContextLock(const ContextLock &) = delete;

        ContextLock &operator=(const ContextLock &) = delete;

        ContextLock(ContextLock &&other) : resource{other.resource}, ownsLock{other.ownsLock} {
            other.ownsLock = false;
        }

        ContextLock &operator=(ContextLock &&other) {
            resource = other.resource;
            ownsLock = other.ownsLock;
            other.ownsLock = false;
            return *this;
        }

        ~ContextLock() {
            if (ownsLock)
                resource->unlock();
        }

        /**
         * @return If this lock owns the context lock on the resource, the destruction of this lock will cause the resource to be unlocked
         */
        constexpr bool OwnsLock() const {
            return ownsLock;
        }

        /**
         * @return If this lock was the first lock on the resource from this context, this corresponds to it being the first usage since any past usages would need to lock
         * @note This is an alias of OwnsLock() with a different name to be more semantically accurate
         */
        constexpr bool IsFirstUsage() const {
            return ownsLock;
        }

        /**
         * @brief Releases the ownership of this lock, the destruction of this lock will no longer unlock the underlying resource
         * @note This will cause IsFirstUsage() to return false regardless of if this is the first usage, this must be handled correctly
         */
        constexpr void Release() {
            ownsLock = false;
        }
    };
}
