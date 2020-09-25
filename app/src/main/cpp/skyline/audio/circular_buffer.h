// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <array>
#include <span>
#include <common.h>

namespace skyline::audio {
    /**
     * @brief This class is used to abstract an array into a circular buffer
     * @tparam Type The type of elements stored in the buffer
     * @tparam Size The maximum size of the circular buffer
     */
    template<typename Type, size_t Size>
    class CircularBuffer {
      private:
        std::array<Type, Size> array{}; //!< The internal array holding the circular buffer
        Type *start{array.begin()}; //!< The start/oldest element of the internal array
        Type *end{array.begin()}; //!< The end/newest element of the internal array
        bool empty{true}; //!< This boolean is used to differentiate between the buffer being full or empty
        Mutex mtx; //!< The mutex ensures that the buffer operations don't overlap

      public:
        /**
         * @brief This reads data from this buffer into the specified buffer
         * @param address The address to write buffer data into
         * @param maxSize The maximum amount of data to write in units of Type
         * @param copyFunction If this is specified, then this is called rather than memcpy
         * @return The amount of data written into the input buffer in units of Type
         */
        inline size_t Read(Type *address, ssize_t maxSize, void copyFunction(Type *, Type *) = {}, ssize_t copyOffset = -1) {
            std::lock_guard guard(mtx);

            if (empty)
                return 0;

            ssize_t size{}, sizeBegin{}, sizeEnd{};

            if (start < end) {
                sizeEnd = std::min(end - start, maxSize);

                size = sizeEnd;
            } else {
                sizeEnd = std::min(array.end() - start, maxSize);
                sizeBegin = std::min(end - array.begin(), maxSize - sizeEnd);

                size = sizeBegin + sizeEnd;
            }

            if (copyFunction && copyOffset) {
                auto sourceEnd = start + ((copyOffset != -1) ? copyOffset : sizeEnd);

                for (auto source = start, destination = address; source < sourceEnd; source++, destination++)
                    copyFunction(source, destination);

                if (copyOffset != -1) {
                    std::memcpy(address + copyOffset, start + copyOffset, (sizeEnd - copyOffset) * sizeof(Type));
                    copyOffset -= sizeEnd;
                }
            } else {
                std::memcpy(address, start, sizeEnd * sizeof(Type));
            }

            address += sizeEnd;

            if (sizeBegin) {
                if (copyFunction && copyOffset) {
                    auto sourceEnd = array.begin() + ((copyOffset != -1) ? copyOffset : sizeBegin);

                    for (auto source = array.begin(), destination = address; source < sourceEnd; source++, destination++)
                        copyFunction(source, destination);

                    if (copyOffset != -1)
                        std::memcpy(array.begin() + copyOffset, address + copyOffset, (sizeBegin - copyOffset) * sizeof(Type));
                } else {
                    std::memcpy(address, array.begin(), sizeBegin * sizeof(Type));
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
         * @brief This appends data from the specified buffer into this buffer
         * @param address The address of the buffer
         * @param size The size of the buffer in units of Type
         */
        inline void Append(Type *address, ssize_t size) {
            std::lock_guard guard(mtx);

            while (size) {
                if (start <= end && end != array.end()) {
                    auto sizeEnd = std::min(array.end() - end, size);
                    std::memcpy(end, address, sizeEnd * sizeof(Type));

                    address += sizeEnd;
                    size -= sizeEnd;

                    end += sizeEnd;
                } else {
                    auto sizePreStart = (end == array.end()) ? std::min(start - array.begin(), size) : std::min(start - end, size);
                    auto sizePostStart = std::min(array.end() - start, size - sizePreStart);

                    if (sizePreStart)
                        std::memcpy((end == array.end()) ? array.begin() : end, address, sizePreStart * sizeof(Type));

                    if (end == array.end())
                        end = array.begin() + sizePreStart;
                    else
                        end += sizePreStart;

                    address += sizePreStart;
                    size -= sizePreStart;

                    if (sizePostStart)
                        std::memcpy(end, address, sizePostStart * sizeof(Type));

                    if (start == array.end())
                        start = array.begin() + sizePostStart;
                    else
                        start += sizePostStart;

                    if (end == array.end())
                        end = array.begin() + sizePostStart;
                    else
                        end += sizePostStart;

                    address += sizePostStart;
                    size -= sizePostStart;
                }

                empty = false;
            }
        }

        /**
         * @brief This appends data from a span to the buffer
         * @param data A span containing the data to be appended
         */
        inline void Append(span<Type> data) {
            Append(data.data(), data.size());
        }
    };
}
