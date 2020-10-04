// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <audio/resampler.h>
#include <audio/adpcm_decoder.h>
#include <audio.h>

namespace skyline::service::audio::IAudioRenderer {
    struct BiquadFilter {
        u8 enable;
        u8 _pad0_;
        u16 b0;
        u16 b1;
        u16 b2;
        u16 a1;
        u16 a2;
    };
    static_assert(sizeof(BiquadFilter) == 0xC);

    struct WaveBuffer {
        u8* pointer;
        u64 size;
        u32 firstSampleOffset;
        u32 lastSampleOffset;
        u8 looping; //!< Whether to loop the buffer
        u8 lastBuffer; //!< Whether this is the last populated buffer
        u16 _unk0_;
        u32 _unk1_;
        u64 adpcmLoopContextPosition;
        u64 adpcmLoopContextSize;
        u64 _unk2_;
    };
    static_assert(sizeof(WaveBuffer) == 0x38);

    struct VoiceIn {
        u32 slot;
        u32 nodeId;
        u8 firstUpdate; //!< Whether this voice is newly added
        u8 acquired; //!< Whether the sample is in use
        skyline::audio::AudioOutState playbackState;
        skyline::audio::AudioFormat format;
        u32 sampleRate;
        u32 priority;
        u32 _unk0_;
        u32 channelCount;
        float pitch;
        float volume;
        std::array<BiquadFilter, 2> biquadFilters;
        u32 appendedWaveBuffersCount;
        u32 baseWaveBufferIndex;
        u32 _unk1_;
        u32* adpcmCoeffs;
        u64 adpcmCoeffsSize;
        u32 destination;
        u32 _pad0_;
        std::array<WaveBuffer, 4> waveBuffers;
        std::array<u32, 6> voiceChannelResourceIds;
        u32 _pad1_[6];
    };
    static_assert(sizeof(VoiceIn) == 0x170);


    struct VoiceOut {
        u64 playedSamplesCount;
        u32 playedWaveBuffersCount;
        u32 voiceDropsCount; //!< The amount of time audio frames have been dropped due to the rendering time limit
    };
    static_assert(sizeof(VoiceOut) == 0x10);

    /**
     * @brief The Voice class manages an audio voice
     */
    class Voice {
      private:
        const DeviceState &state;
        std::array<WaveBuffer, 4> waveBuffers;
        std::vector<i16> samples; //!< A vector containing processed sample data
        skyline::audio::Resampler resampler; //!< The resampler object used for changing the sample rate of a wave buffer's stream
        std::optional<skyline::audio::AdpcmDecoder> adpcmDecoder;

        bool acquired{false}; //!< If the voice is in use
        bool bufferReload{true};
        u8 bufferIndex{}; //!< The index of the wave buffer currently in use
        u32 sampleOffset{}; //!< The offset in the sample data of the current wave buffer
        u32 sampleRate{};
        u8 channelCount{};
        skyline::audio::AudioOutState playbackState{skyline::audio::AudioOutState::Stopped};
        skyline::audio::AudioFormat format{skyline::audio::AudioFormat::Invalid};

        /**
         * @brief Updates the sample buffer with data from the current wave buffer and processes it
         */
        void UpdateBuffers();

        /**
         * @brief Sets the current wave buffer index to use
         * @param index The index to use
         */
        void SetWaveBufferIndex(u8 index);

      public:
        VoiceOut output{};
        float volume{};

        Voice(const DeviceState &state);

        /**
         * @brief Reads the input voice data from the guest and sets internal data based off it
         * @param input The input data struct from guest
         */
        void ProcessInput(const VoiceIn &input);

        /**
         * @brief Obtains the voices audio sample data, updating it if required
         * @param maxSamples The maximum amount of samples the output buffer should contain
         * @return A vector of I16 PCM sample data
         */
        std::vector<i16> &GetBufferData(u32 maxSamples, u32 &outOffset, u32 &outSize);

        /**
         * @return If the voice is currently playable
         */
        inline bool Playable() {
            return acquired && playbackState == skyline::audio::AudioOutState::Started && waveBuffers[bufferIndex].size != 0;
        }
    };
}
