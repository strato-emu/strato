// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <array>
#include <audio/resampler.h>
#include <audio/adpcm_decoder.h>
#include <audio.h>
#include <common.h>

namespace skyline::service::audio::IAudioRenderer {
    /**
     * @brief This stores data for configuring a biquadratic filter
     */
    struct BiquadFilter {
        u8 enable; //!< Whether to enable the filter
        u8 _pad0_;
        u16 b0; //!< The b0 constant
        u16 b1; //!< The b1 constant
        u16 b2; //!< The b2 constant
        u16 a1; //!< The a1 constant
        u16 a2; //!< The a2 constant
    };
    static_assert(sizeof(BiquadFilter) == 0xC);

    /**
     * @brief This stores information of a wave buffer of samples
     */
    struct WaveBuffer {
        u64 address; //!< The address of the wave buffer in guest memory
        u64 size; //!< The size of the wave buffer
        u32 firstSampleOffset; //!< The offset of the first sample in the buffer
        u32 lastSampleOffset; //!< The offset of the last sample in the buffer
        u8 looping; //!< Whether to loop the buffer
        u8 lastBuffer; //!< Whether this is the last populated buffer
        u16 _unk0_;
        u32 _unk1_;
        u64 adpcmLoopContextPosition; //!< The position of the ADPCM loop context data
        u64 adpcmLoopContextSize; //!< The size of the ADPCM loop context data
        u64 _unk2_;
    };
    static_assert(sizeof(WaveBuffer) == 0x38);

    /**
     * @brief This is in input containing the configuration of a voice
     */
    struct VoiceIn {
        u32 slot; //!< The slot of the voice
        u32 nodeId; //!< The node ID of the voice
        u8 firstUpdate; //!< Whether this voice is new
        u8 acquired; //!< Whether the sample is in use
        skyline::audio::AudioOutState playbackState; //!< The playback state
        skyline::audio::AudioFormat format; //!< The sample format
        u32 sampleRate; //!< The sample rate
        u32 priority; //!< The priority for this voice
        u32 _unk0_;
        u32 channelCount; //!< The amount of channels the wave buffers contain
        float pitch; //!< The pitch to play wave data at
        float volume; //!< The volume to play wave data at
        std::array<BiquadFilter, 2> biquadFilters; //!< Biquadratic filter configurations
        u32 appendedWaveBuffersCount; //!< The amount of buffers appended
        u32 baseWaveBufferIndex; //!< The start index of the buffer to use
        u32 _unk1_;
        u64 adpcmCoeffsPosition; //!< The ADPCM coefficient position in wave data
        u64 adpcmCoeffsSize; //!< The size of the ADPCM coefficient configuration data
        u32 destination; //!< The voice destination address
        u32 _pad0_;
        std::array<WaveBuffer, 4> waveBuffers; //!< The wave buffers for the voice
        std::array<u32, 6> voiceChannelResourceIds; //!< A list of IDs corresponding to each channel
        u32 _pad1_[6];
    };
    static_assert(sizeof(VoiceIn) == 0x170);


    /**
     * @brief This is returned to inform the guest of the state of a voice
     */
    struct VoiceOut {
        u64 playedSamplesCount; //!< The amount of samples played
        u32 playedWaveBuffersCount; //!< The amount of wave buffers fully played
        u32 voiceDropsCount; //!< The amount of time audio frames have been dropped due to the rendering time limit
    };
    static_assert(sizeof(VoiceOut) == 0x10);

    /**
    * @brief The Voice class manages an audio voice
    */
    class Voice {
      private:
        const DeviceState &state; //!< The emulator state object
        std::array<WaveBuffer, 4> waveBuffers; //!< An array containing the state of all four wave buffers
        std::vector<i16> samples; //!< A vector containing processed sample data
        skyline::audio::Resampler resampler; //!< The resampler object used for changing the sample rate of a stream
        std::optional<skyline::audio::AdpcmDecoder> adpcmDecoder; //!< The decoder object used for decoding ADPCM encoded samples

        bool acquired{false}; //!< If the voice is in use
        bool bufferReload{true}; //!< If the buffer needs to be updated
        u8 bufferIndex{}; //!< The index of the wave buffer currently in use
        u32 sampleOffset{}; //!< The offset in the sample data of the current wave buffer
        u32 sampleRate{}; //!< The sample rate of the sample data
        u8 channelCount{}; //!< The amount of channels in the sample data
        skyline::audio::AudioOutState playbackState{skyline::audio::AudioOutState::Stopped}; //!< The playback state of the voice
        skyline::audio::AudioFormat format{skyline::audio::AudioFormat::Invalid}; //!< The format used for guest audio data

        /**
         * @brief This updates the sample buffer with data from the current wave buffer and processes it
         */
        void UpdateBuffers();

        /**
         * @brief Sets the current wave buffer index to use
         * @param index The index to use
         */
        void SetWaveBufferIndex(u8 index);

      public:
        VoiceOut output{}; //!< The current output state
        float volume{}; //!< The volume of the voice

        Voice(const DeviceState &state);

        /**
         * @brief This reads the input voice data from the guest and sets internal data based off it
         * @param input The input data struct from guest
         */
        void ProcessInput(const VoiceIn &input);

        /**
         * @brief This obtains the voices audio sample data, updating it if required
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
