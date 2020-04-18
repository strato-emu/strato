// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IAudioOut.h"

namespace skyline::service::audio {
    IAudioOut::IAudioOut(const DeviceState &state, ServiceManager &manager, const u8 channelCount, const u32 sampleRate) : sampleRate(sampleRate), channelCount(channelCount), releaseEvent(std::make_shared<type::KEvent>(state)), BaseService(state, manager, Service::audio_IAudioOut, "audio:IAudioOut", {
        {0x0, SFUNC(IAudioOut::GetAudioOutState)},
        {0x1, SFUNC(IAudioOut::StartAudioOut)},
        {0x2, SFUNC(IAudioOut::StopAudioOut)},
        {0x3, SFUNC(IAudioOut::AppendAudioOutBuffer)},
        {0x4, SFUNC(IAudioOut::RegisterBufferEvent)},
        {0x5, SFUNC(IAudioOut::GetReleasedAudioOutBuffer)},
        {0x6, SFUNC(IAudioOut::ContainsAudioOutBuffer)}
    }) {
        track = state.audio->OpenTrack(channelCount, constant::SampleRate, [this]() { this->releaseEvent->Signal(); });
    }

    IAudioOut::~IAudioOut() {
        state.audio->CloseTrack(track);
    }

    void IAudioOut::GetAudioOutState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(static_cast<u32>(track->playbackState));
    }

    void IAudioOut::StartAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.logger->Debug("IAudioOut: Start playback");
        track->Start();
    }

    void IAudioOut::StopAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.logger->Debug("IAudioOut: Stop playback");
        track->Stop();
    }

    void IAudioOut::AppendAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct Data {
            u64 nextBufferPtr;
            u64 sampleBufferPtr;
            u64 sampleCapacity;
            u64 sampleSize;
            u64 sampleOffset;
        } data = state.process->GetObject<Data>(request.inputBuf.at(0).address);
        auto tag = request.Pop<u64>();

        state.logger->Debug("IAudioOut: Appending buffer with address: 0x{:X}, size: 0x{:X}", data.sampleBufferPtr, data.sampleSize);

        if (sampleRate != constant::SampleRate) {
            tmpSampleBuffer.resize(data.sampleSize / sizeof(i16));
            state.process->ReadMemory(tmpSampleBuffer.data(), data.sampleBufferPtr, data.sampleSize);
            resampler.ResampleBuffer(tmpSampleBuffer, static_cast<double>(sampleRate) / constant::SampleRate, channelCount);

            track->AppendBuffer(tag, tmpSampleBuffer);
        } else {
            track->AppendBuffer(tag, state.process->GetPointer<i16>(data.sampleBufferPtr), data.sampleSize);
        }
    }

    void IAudioOut::RegisterBufferEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle = state.process->InsertItem(releaseEvent);
        state.logger->Debug("IAudioOut: Buffer Release Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
    }

    void IAudioOut::GetReleasedAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto maxCount = static_cast<u32>(request.outputBuf.at(0).size >> 3);
        std::vector<u64> releasedBuffers = track->GetReleasedBuffers(maxCount);
        auto count = static_cast<u32>(releasedBuffers.size());

        // Fill rest of output buffer with zeros
        releasedBuffers.resize(maxCount, 0);
        state.process->WriteMemory(releasedBuffers.data(), request.outputBuf.at(0).address, request.outputBuf.at(0).size);

        response.Push<u32>(count);
    }

    void IAudioOut::ContainsAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto tag = request.Pop<u64>();

        response.Push(static_cast<u32>(track->ContainsBuffer(tag)));
    }
}
