// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <oboe/Oboe.h>
#include <common.h>
#include <array>

namespace skyline {
    namespace constant {
        constexpr auto SampleRate = 48000; //!< The common sampling rate to use for audio output
        constexpr auto ChannelCount = 2; //!< The common amount of channels to use for audio output
        constexpr auto PcmFormat = oboe::AudioFormat::I16; //!< The common PCM data format to use for audio output
        constexpr size_t MixBufferSize = 960; //!< The size of the mix buffer by default
    };

    namespace audio {
        /**
         * @brief The available PCM stream formats
         */
        enum class AudioFormat : u8 {
            Invalid = 0, //!< An invalid PCM format
            Int8 = 1, //!< 8 bit integer PCM
            Int16 = 2, //!< 16 bit integer PCM
            Int24 = 3, //!< 24 bit integer PCM
            Int32 = 4, //!< 32 bit integer PCM
            Float = 5, //!< Floating point PCM
            ADPCM = 6 //!< Adaptive differential PCM
        };

        /**
         * @brief The state of an audio track
         */
        enum class AudioOutState : u8 {
            Started = 0, //!< Stream is started and is playing
            Stopped = 1, //!< Stream is stopped, there are no samples left to play
            Paused = 2 //!< Stream is paused, some samples may not have been played yet
        };

        /**
         * @brief This stores information about pushed buffers
         */
        struct BufferIdentifier {
            u64 tag; //!< The tag of the buffer
            u64 finalSample; //!< The final sample this buffer will be played in
            bool released; //!< If the buffer has been released
        };

        /**
         * @brief This saturates the specified value according to the numeric limits of Out
         * @tparam Out The return value type and the numeric limit clamp
         * @tparam Intermediate The intermediate type that is converted to from In before clamping
         * @tparam In The input value type
         * @param value The value to saturate
         * @return The saturated value
         */
        template<typename Out, typename Intermediate, typename In>
        inline Out Saturate(In value) {
            return static_cast<Out>(std::clamp(static_cast<Intermediate>(value), static_cast<Intermediate>(std::numeric_limits<Out>::min()), static_cast<Intermediate>(std::numeric_limits<Out>::max())));
        }

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
             * @param function If this is specified, then this is called rather than memcpy
             * @return The amount of data written into the input buffer in units of Type
             */
            inline size_t Read(Type *address, ssize_t maxSize, void function(Type *, Type *) = {}, ssize_t copyOffset = -1) {
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

                if (function && copyOffset) {
                    auto sourceEnd = start + ((copyOffset != -1) ? copyOffset : sizeEnd);

                    for (auto source = start, destination = address; source < sourceEnd; source++, destination++)
                        function(source, destination);

                    if (copyOffset != -1) {
                        std::memcpy(address + copyOffset, start + copyOffset, (sizeEnd - copyOffset) * sizeof(Type));
                        copyOffset -= sizeEnd;
                    }
                } else {
                    std::memcpy(address, start, sizeEnd * sizeof(Type));
                }

                address += sizeEnd;

                if (sizeBegin) {
                    if (function && copyOffset) {
                        auto sourceEnd = array.begin() + ((copyOffset != -1) ? copyOffset : sizeBegin);

                        for (auto source = array.begin(), destination = address; source < sourceEnd; source++, destination++)
                            function(source, destination);

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
             * @brief This appends data from a vector to the buffer
             * @param sampleData A reference to a vector containing the data to be appended
             */
            inline void Append(const std::vector<Type> &data) {
                Append(const_cast<Type *>(data.data()), data.size());
            }
        };
    }
}
