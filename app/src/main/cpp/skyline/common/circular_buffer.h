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
        inline size_t Read(span<Type> buffer, void copyFunction(Type *, Type *) = {}, ssize_t copyOffset = -1) {
            std::lock_guard guard(mtx);

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

            if (copyFunction && copyOffset) {
                auto sourceEnd{start + ((copyOffset != -1) ? copyOffset : sizeEnd)};

                for (auto source{start}, destination{pointer}; source < sourceEnd; source++, destination++)
                    copyFunction(source, destination);

                if (copyOffset != -1) {
                    std::memcpy(pointer + copyOffset, start + copyOffset, (sizeEnd - copyOffset) * sizeof(Type));
                    copyOffset -= sizeEnd;
                }
            } else {
                std::memcpy(pointer, start, sizeEnd * sizeof(Type));
            }

            pointer += sizeEnd;

            if (sizeBegin) {
                if (copyFunction && copyOffset) {
                    auto sourceEnd{array.begin() + ((copyOffset != -1) ? copyOffset : sizeBegin)};

                    for (auto source{array.begin()}, destination{pointer}; source < sourceEnd; source++, destination++)
                        copyFunction(source, destination);

                    if (copyOffset != -1)
                        std::memcpy(array.begin() + copyOffset, pointer + copyOffset, (sizeBegin - copyOffset) * sizeof(Type));
                } else {
                    std::memcpy(pointer, array.begin(), sizeBegin * sizeof(Type));
                }

                start = array.begin() + sizeBegin;
            } else {
                start += sizeEnd;
            }

            if (start == end)
                empty = true;

            return static_cast<size_t>(size);
        }

        /**
         * @brief Appends data from the specified buffer into this buffer
         */
        inline void Append(span<Type> buffer) {
            std::lock_guard guard(mtx);

            Type *pointer{buffer.data()};
            ssize_t size{static_cast<ssize_t>(buffer.size())};
            while (size) {
                if (start <= end && end != array.end()) {
                    auto sizeEnd{std::min(array.end() - end, size)};
                    std::memcpy(end, pointer, sizeEnd * sizeof(Type));

                    pointer += sizeEnd;
                    size -= sizeEnd;

                    end += sizeEnd;
                } else {
                    auto sizePreStart{(end == array.end()) ? std::min(start - array.begin(), size) : std::min(start - end, size)};
                    auto sizePostStart{std::min(array.end() - start, size - sizePreStart)};

                    if (sizePreStart)
                        std::memcpy((end == array.end()) ? array.begin() : end, pointer, sizePreStart * sizeof(Type));

                    if (end == array.end())
                        end = array.begin() + sizePreStart;
                    else
                        end += sizePreStart;

                    pointer += sizePreStart;
                    size -= sizePreStart;

                    if (sizePostStart)
                        std::memcpy(end, pointer, sizePostStart * sizeof(Type));

                    if (start == array.end())
                        start = array.begin() + sizePostStart;
                    else
                        start += sizePostStart;

                    if (end == array.end())
                        end = array.begin() + sizePostStart;
                    else
                        end += sizePostStart;

                    pointer += sizePostStart;
                    size -= sizePostStart;
                }

                empty = false;
            }
        }
    };
}
