// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline {
    /**
     * @brief An abstraction of an array into a circular buffer
     * @tparam Type The type of elements stored in the buffer
     * @tparam Size The maximum size of the circular buffer
     * @url https://en.wikipedia.org/wiki/Circular_buffer
     */
    template<typename Type, size_t Size>
    class CircularBuffer {
      private:
        std::array<Type, Size> array{}; //!< The internal array holding the circular buffer
        Type *start{array.begin()}; //!< The start/oldest element of the internal array
        Type *end{array.begin()}; //!< The end/newest element of the internal array
        bool empty{true}; //!< If the buffer is full or empty, as start == end can mean either
        std::mutex mtx; //!< Synchronizes buffer operations so they don't overlap

      public:
        /**
         * @brief Reads data from this buffer into the specified buffer
         * @param copyFunction If this is specified, then this is called rather than memcpy
         * @param copyOffset The offset into the buffer after which to use memcpy rather than copyFunction, -1 will use it for the entire buffer
         * @return The amount of data written into the input buffer in units of Type
         */
        template<typename CopyFunc>
        size_t Read(span <Type> buffer, CopyFunc copyFunction) {
            std::scoped_lock guard{mtx};

            if (empty)
                return 0;

            Type *pointer{buffer.data()};
            ssize_t maxSize{static_cast<ssize_t>(buffer.size())}, size{}, sizeBegin{}, sizeEnd{};

            if (start < end) {
                sizeEnd = std::min(end - start, maxSize);

                size = sizeEnd;
            } else {
                sizeEnd = std::min(array.end() - start, maxSize);
                sizeBegin = std::min(end - array.begin(), maxSize - sizeEnd);

                size = sizeBegin + sizeEnd;
            }

            {
                auto sourceEnd{start + sizeEnd};

                for (; start < sourceEnd; start++, pointer++)
                    copyFunction(start, pointer);
            }

            if (sizeBegin) {
                start = array.begin();
                auto sourceEnd{array.begin() + sizeBegin};

                for (; start < sourceEnd; start++, pointer++)
                    copyFunction(start, pointer);
            }

            if (start == end)
                empty = true;

            return static_cast<size_t>(size);
        }

        /**
         * @brief Appends data from the specified buffer into this buffer
         */
        void Append(span <Type> buffer) {
            std::scoped_lock guard{mtx};

            Type *pointer{buffer.data()};
            ssize_t remainingSize{static_cast<ssize_t>(buffer.size())};
            while (remainingSize) {
                ssize_t writeSize{std::min([&]() {
                    if (start <= end)
                        return array.end() - end;
                    else
                        return start - end - 1;
                }(), remainingSize)};

                if (writeSize) {
                    std::memcpy(end, pointer, static_cast<size_t>(writeSize) * sizeof(Type));
                    remainingSize -= writeSize;
                    end += writeSize;
                    pointer += writeSize;
                }

                if (end == array.end())
                    end = array.begin();
                empty = false;
            }
        }
    };
}
