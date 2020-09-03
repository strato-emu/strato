// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IAudioRenderer.h"

namespace skyline::service::audio::IAudioRenderer {
    IAudioRenderer::IAudioRenderer(const DeviceState &state, ServiceManager &manager, AudioRendererParameters &parameters)
        : systemEvent(std::make_shared<type::KEvent>(state)), parameters(parameters), BaseService(state, manager, {
        {0x0, SFUNC(IAudioRenderer::GetSampleRate)},
        {0x1, SFUNC(IAudioRenderer::GetSampleCount)},
        {0x2, SFUNC(IAudioRenderer::GetMixBufferCount)},
        {0x3, SFUNC(IAudioRenderer::GetState)},
        {0x4, SFUNC(IAudioRenderer::RequestUpdate)},
        {0x5, SFUNC(IAudioRenderer::Start)},
        {0x6, SFUNC(IAudioRenderer::Stop)},
        {0x7, SFUNC(IAudioRenderer::QuerySystemEvent)},
        {0xA, SFUNC(IAudioRenderer::RequestUpdate)},
    }) {
        track = state.audio->OpenTrack(constant::ChannelCount, constant::SampleRate, []() {});
        track->Start();

        memoryPools.resize(parameters.effectCount + parameters.voiceCount * 4);
        effects.resize(parameters.effectCount);
        voices.resize(parameters.voiceCount, Voice(state));

        // Fill track with empty samples that we will triple buffer
        track->AppendBuffer(0);
        track->AppendBuffer(1);
        track->AppendBuffer(2);
    }

    IAudioRenderer::~IAudioRenderer() {
        state.audio->CloseTrack(track);
    }

    Result IAudioRenderer::GetSampleRate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(parameters.sampleRate);
        return {};
    }

    Result IAudioRenderer::GetSampleCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(parameters.sampleCount);
        return {};
    }

    Result IAudioRenderer::GetMixBufferCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(parameters.subMixCount);
        return {};
    }

    Result IAudioRenderer::GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(static_cast<u32>(playbackState));
        return {};
    }

    Result IAudioRenderer::RequestUpdate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto inputAddress{request.inputBuf.at(0).address};

        auto inputHeader{state.process->GetObject<UpdateDataHeader>(inputAddress)};
        revisionInfo.SetUserRevision(inputHeader.revision);
        inputAddress += sizeof(UpdateDataHeader);
        inputAddress += inputHeader.behaviorSize; // Unused

        auto memoryPoolCount{memoryPools.size()};
        std::vector<MemoryPoolIn> memoryPoolsIn(memoryPoolCount);
        state.process->ReadMemory(memoryPoolsIn.data(), inputAddress, memoryPoolCount * sizeof(MemoryPoolIn));
        inputAddress += inputHeader.memoryPoolSize;

        for (auto i = 0; i < memoryPoolsIn.size(); i++)
            memoryPools[i].ProcessInput(memoryPoolsIn[i]);

        inputAddress += inputHeader.voiceResourceSize;
        std::vector<VoiceIn> voicesIn(parameters.voiceCount);
        state.process->ReadMemory(voicesIn.data(), inputAddress, parameters.voiceCount * sizeof(VoiceIn));
        inputAddress += inputHeader.voiceSize;

        for (auto i = 0; i < voicesIn.size(); i++)
            voices[i].ProcessInput(voicesIn[i]);

        std::vector<EffectIn> effectsIn(parameters.effectCount);
        state.process->ReadMemory(effectsIn.data(), inputAddress, parameters.effectCount * sizeof(EffectIn));

        for (auto i = 0; i < effectsIn.size(); i++)
            effects[i].ProcessInput(effectsIn[i]);

        UpdateAudio();
        systemEvent->Signal();

        UpdateDataHeader outputHeader{
            .revision = constant::RevMagic,
            .behaviorSize = 0xB0,
            .memoryPoolSize = (parameters.effectCount + parameters.voiceCount * 4) * static_cast<u32>(sizeof(MemoryPoolOut)),
            .voiceSize = parameters.voiceCount * static_cast<u32>(sizeof(VoiceOut)),
            .effectSize = parameters.effectCount * static_cast<u32>(sizeof(EffectOut)),
            .sinkSize = parameters.sinkCount * 0x20,
            .performanceManagerSize = 0x10,
            .elapsedFrameCountInfoSize = 0x0
        };

        if (revisionInfo.ElapsedFrameCountSupported())
            outputHeader.elapsedFrameCountInfoSize = 0x10;

        outputHeader.totalSize = sizeof(UpdateDataHeader) +
            outputHeader.behaviorSize +
            outputHeader.memoryPoolSize +
            outputHeader.voiceSize +
            outputHeader.effectSize +
            outputHeader.sinkSize +
            outputHeader.performanceManagerSize +
            outputHeader.elapsedFrameCountInfoSize;

        u64 outputAddress = request.outputBuf.at(0).address;

        state.process->WriteMemory(outputHeader, outputAddress);
        outputAddress += sizeof(UpdateDataHeader);

        for (const auto &memoryPool : memoryPools) {
            state.process->WriteMemory(memoryPool.output, outputAddress);
            outputAddress += sizeof(MemoryPoolOut);
        }

        for (const auto &voice : voices) {
            state.process->WriteMemory(voice.output, outputAddress);
            outputAddress += sizeof(VoiceOut);
        }

        for (const auto &effect : effects) {
            state.process->WriteMemory(effect.output, outputAddress);
            outputAddress += sizeof(EffectOut);
        }

        return {};
    }

    void IAudioRenderer::UpdateAudio() {
        auto released{track->GetReleasedBuffers(2)};

        for (auto &tag : released) {
            MixFinalBuffer();
            track->AppendBuffer(tag, sampleBuffer);
        }
    }

    void IAudioRenderer::MixFinalBuffer() {
        u32 writtenSamples = 0;

        for (auto &voice : voices) {
            if (!voice.Playable())
                continue;

            u32 bufferOffset{};
            u32 pendingSamples = constant::MixBufferSize;

            while (pendingSamples > 0) {
                u32 voiceBufferOffset{};
                u32 voiceBufferSize{};
                auto &voiceSamples{voice.GetBufferData(pendingSamples, voiceBufferOffset, voiceBufferSize)};

                if (voiceBufferSize == 0)
                    break;

                pendingSamples -= voiceBufferSize / constant::ChannelCount;

                for (auto index{voiceBufferOffset}; index < voiceBufferOffset + voiceBufferSize; index++) {
                    if (writtenSamples == bufferOffset) {
                        sampleBuffer[bufferOffset] = skyline::audio::Saturate<i16, i32>(voiceSamples[index] * voice.volume);

                        writtenSamples++;
                    } else {
                        sampleBuffer[bufferOffset] = skyline::audio::Saturate<i16, i32>(sampleBuffer[bufferOffset] + (voiceSamples[index] * voice.volume));
                    }

                    bufferOffset++;
                }
            }
        }
    }

    Result IAudioRenderer::Start(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        playbackState = skyline::audio::AudioOutState::Started;
        return {};
    }

    Result IAudioRenderer::Stop(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        playbackState = skyline::audio::AudioOutState::Stopped;
        return {};
    }

    Result IAudioRenderer::QuerySystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(systemEvent)};
        state.logger->Debug("Audren System Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }
}
