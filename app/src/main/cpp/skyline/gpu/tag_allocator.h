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
     * @note This will unlock the tag when the scope is exited, **if** it locked the tag in the first place
     */
    template<typename T>
    class ContextLock {
      private:
        T &resource;

      public:
        bool isFirst; //!< If this was the first lock for this context

        ContextLock(ContextTag tag, T &resource) : resource{resource}, isFirst{resource.LockWithTag(tag)} {}

        ContextLock(const ContextLock &) = delete;

        ContextLock(ContextLock &&other) noexcept : resource{other.resource}, isFirst{other.isFirst} {
            other.isFirst = false;
        }

        ~ContextLock() {
            if (isFirst)
                resource.unlock();
        }
    };
}
