// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IAudioRenderer.h"

namespace skyline::service::audio::IAudioRenderer {
    IAudioRenderer::IAudioRenderer(const DeviceState &state, ServiceManager &manager, AudioRendererParameters &parameters)
        : systemEvent(std::make_shared<type::KEvent>(state)), parameters(parameters), BaseService(state, manager) {
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
        auto input{request.inputBuf.at(0).data()};

        auto inputHeader{*reinterpret_cast<UpdateDataHeader *>(input)};
        revisionInfo.SetUserRevision(inputHeader.revision);
        input += sizeof(UpdateDataHeader);
        input += inputHeader.behaviorSize; // Unused

        span memoryPoolsIn(reinterpret_cast<MemoryPoolIn*>(input), memoryPools.size());
        input += inputHeader.memoryPoolSize;
        for (auto i = 0; i < memoryPools.size(); i++)
            memoryPools[i].ProcessInput(memoryPoolsIn[i]);

        input += inputHeader.voiceResourceSize;

        span voicesIn(reinterpret_cast<VoiceIn*>(input), parameters.voiceCount);
        input += inputHeader.voiceSize;
        for (auto i = 0; i < voicesIn.size(); i++)
            voices[i].ProcessInput(voicesIn[i]);

        span effectsIn(reinterpret_cast<EffectIn*>(input), parameters.effectCount);
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

        auto output{request.outputBuf.at(0).data()};

        *reinterpret_cast<UpdateDataHeader*>(output) = outputHeader;
        output += sizeof(UpdateDataHeader);

        for (const auto &memoryPool : memoryPools) {
            *reinterpret_cast<MemoryPoolOut*>(output) = memoryPool.output;
            output += sizeof(MemoryPoolOut);
        }

        for (const auto &voice : voices) {
            *reinterpret_cast<VoiceOut*>(output) = voice.output;
            output += sizeof(VoiceOut);
        }

        for (const auto &effect : effects) {
            *reinterpret_cast<EffectOut*>(output) = effect.output;
            output += sizeof(EffectOut);
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
