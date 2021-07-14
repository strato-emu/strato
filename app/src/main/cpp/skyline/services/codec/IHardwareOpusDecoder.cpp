// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/timesrv/common.h>
#include <kernel/types/KProcess.h>

#include "IHardwareOpusDecoder.h"

namespace skyline::service::codec {
    size_t CalculateOutBufferSize(i32 sampleRate, i32 channelCount, i32 frameSize) {
        return util::AlignUp(frameSize * channelCount / (OpusFullbandSampleRate / sampleRate), 0x40);
    }

    IHardwareOpusDecoder::IHardwareOpusDecoder(const DeviceState &state, ServiceManager &manager, i32 sampleRate, i32 channelCount, u32 workBufferSize, KHandle workBufferHandle)
        : BaseService(state, manager),
          sampleRate(sampleRate),
          channelCount(channelCount),
          workBuffer(state.process->GetHandle<kernel::type::KTransferMemory>(workBufferHandle)),
          decoderOutputBufferSize(CalculateOutBufferSize(sampleRate, channelCount, MaxFrameSizeNormal)) {
        if (workBufferSize < decoderOutputBufferSize)
            throw exception("Work Buffer doesn't have adequate space for Opus Decoder: 0x{:X} (Required: 0x{:X})", workBufferSize, decoderOutputBufferSize);

        // We utilize the guest-supplied work buffer for allocating the OpusDecoder object into
        decoderState = reinterpret_cast<OpusDecoder *>(workBuffer->host.ptr);

        int result{opus_decoder_init(decoderState, sampleRate, channelCount)};
        if (result != OPUS_OK)
            throw OpusException(result);
    }

    Result IHardwareOpusDecoder::DecodeInterleavedOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return DecodeInterleavedImpl(request, response);
    }

    Result IHardwareOpusDecoder::DecodeInterleavedWithPerfOld(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return DecodeInterleavedImpl(request, response, true);
    }

    Result IHardwareOpusDecoder::DecodeInterleaved(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        bool reset{static_cast<bool>(request.Pop<u8>())};
        if (reset)
            ResetContext();

        return DecodeInterleavedImpl(request, response, true);
    }

    void IHardwareOpusDecoder::ResetContext() {
        opus_decoder_ctl(decoderState, OPUS_RESET_STATE);
    }

    Result IHardwareOpusDecoder::DecodeInterleavedImpl(ipc::IpcRequest &request, ipc::IpcResponse &response, bool writeDecodeTime) {
        auto dataIn{request.inputBuf.at(0)};
        auto dataOut{request.outputBuf.at(0).cast<opus_int16>()};

        if (dataIn.size() <= sizeof(OpusDataHeader))
            throw exception("Incorrect Opus data size: 0x{:X} (Should be > 0x{:X})", dataIn.size(), sizeof(OpusDataHeader));

        u32 opusPacketSize{dataIn.as<OpusDataHeader>().GetPacketSize()};
        i32 requiredInSize{static_cast<i32>(opusPacketSize + sizeof(OpusDataHeader))};
        if (opusPacketSize > MaxInputBufferSize || dataIn.size() < requiredInSize)
            throw exception("Opus packet size mismatch: 0x{:X} (Requested: 0x{:X})", dataIn.size() - sizeof(OpusDataHeader), opusPacketSize);

        // Skip past the header in the input buffer to get the Opus packet
        auto sampleDataIn = dataIn.subspan(sizeof(OpusDataHeader));

        auto perfTimer{timesrv::TimeSpanType::FromNanoseconds(util::GetTimeNs())};
        i32 decodedCount{opus_decode(decoderState, sampleDataIn.data(), opusPacketSize, dataOut.data(), decoderOutputBufferSize, false)};
        perfTimer = timesrv::TimeSpanType::FromNanoseconds(util::GetTimeNs()) - perfTimer;

        if (decodedCount < 0)
            throw OpusException(decodedCount);

        response.Push(requiredInSize); // Decoded data size is equal to opus packet size + header
        response.Push(decodedCount);
        if (writeDecodeTime)
            response.Push<u64>(perfTimer.Microseconds());

        return {};
    }
}
