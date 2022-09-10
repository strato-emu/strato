// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <opus.h>

#include <common.h>
#include <services/base_service.h>
#include <kernel/types/KTransferMemory.h>

namespace skyline::service::codec {
    /**
     * @return The required output buffer size for decoding an Opus stream with the given parameters
     */
    u32 CalculateOutBufferSize(i32 sampleRate, i32 channelCount, i32 frameSize);

    static constexpr i32 OpusFullbandSampleRate{48000};
    static constexpr i32 MaxFrameSizeNormal{static_cast<u32>(OpusFullbandSampleRate * 0.040f)}; //!< 40ms frame size limit for normal decoders
    static constexpr i32 MaxFrameSizeEx{static_cast<u32>(OpusFullbandSampleRate * 0.120f)}; //!< 120ms frame size limit for ex decoders added in 12.0.0
    static constexpr u32 MaxInputBufferSize{0x600}; //!< Maximum allocated size of the input buffer

    /**
     * @note The Switch has a HW Opus Decoder which this service would interface with, we emulate it using libopus with CPU-decoding
     * @url https://switchbrew.org/wiki/Audio_services#IHardwareOpusDecoder
     */
    class IHardwareOpusDecoder : public BaseService {
      private:
        std::shared_ptr<kernel::type::KTransferMemory> workBuffer;
        OpusDecoder *decoderState{};
        i32 sampleRate;
        i32 channelCount;
        u32 decoderOutputBufferSize;

        /**
         * @brief Holds information about the Opus packet to be decoded
         * @note These fields are big-endian
         * @url https://github.com/switchbrew/libnx/blob/c5a9a909a91657a9818a3b7e18c9b91ff0cbb6e3/nx/include/switch/services/hwopus.h#L19
         */
        struct OpusDataHeader {
            u32 sizeBe; //!< Size of the packet following this header
            u32 finalRangeBe; //!< Final range of the codec encoder's entropy coder (can be zero)

            i32 GetPacketSize() {
                return static_cast<i32>(util::SwapEndianness(sizeBe));
            }
        };
        static_assert(sizeof(OpusDataHeader) == 0x8);

        /**
         * @brief Resets the Opus decoder's internal state
         */
        void ResetContext();

        /**
         * @brief Decodes Opus source data via libopus
         */
        Result DecodeInterleavedImpl(ipc::IpcRequest &request, ipc::IpcResponse &response, bool writeDecodeTime = false);

      public:
        IHardwareOpusDecoder(const DeviceState &state, ServiceManager &manager, i32 sampleRate, i32 channelCount, u32 workBufferSize, KHandle workBufferHandle, bool isIsLargerSize = false);

        /**
         * @brief Decodes the Opus source data, returns decoded data size and decoded sample count
         * @url https://switchbrew.org/wiki/Audio_services#DecodeInterleavedOld
         */
        Result DecodeInterleavedOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Decodes the Opus source data, returns decoded data size, decoded sample count and decode time in microseconds
         * @url https://switchbrew.org/wiki/Audio_services#DecodeInterleavedWithPerfOld
         */
        Result DecodeInterleavedWithPerfOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Decodes the Opus source data, returns decoded data size, decoded sample count and decode time in microseconds
         * @note The bool flag indicates whether or not to reset the decoder context
         * @url https://switchbrew.org/wiki/Audio_services#DecodeInterleaved
         */
        Result DecodeInterleaved(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IHardwareOpusDecoder, DecodeInterleavedOld),
            SFUNC(0x4, IHardwareOpusDecoder, DecodeInterleavedWithPerfOld),
            SFUNC(0x6, IHardwareOpusDecoder, DecodeInterleaved), // DecodeInterleavedWithPerfAndResetOld is effectively the same as DecodeInterleaved
            SFUNC(0x8, IHardwareOpusDecoder, DecodeInterleaved),
        )
    };

    /**
     * @brief A wrapper around libopus error codes for exceptions
     */
    class OpusException : public exception {
      public:
        OpusException(int errorCode) : exception("Opus failed with error code {}: {}", errorCode, opus_strerror(errorCode)) {}
    };
}
