#include <kernel/types/KProcess.h>
#include "IAudioRenderer.h"

namespace skyline::service::audio::IAudioRenderer {
    IAudioRenderer::IAudioRenderer(const DeviceState &state, ServiceManager &manager, AudioRendererParams &params)
        : releaseEvent(std::make_shared<type::KEvent>(state)), rendererParams(params), memoryPoolCount(params.effectCount + params.voiceCount * 4), samplesPerBuffer(state.settings->GetInt("audren_buffer_size")), BaseService(state, manager, Service::audio_IAudioRenderer, "audio:IAudioRenderer", {
        {0x0, SFUNC(IAudioRenderer::GetSampleRate)},
        {0x1, SFUNC(IAudioRenderer::GetSampleCount)},
        {0x2, SFUNC(IAudioRenderer::GetMixBufferCount)},
        {0x3, SFUNC(IAudioRenderer::GetState)},
        {0x4, SFUNC(IAudioRenderer::RequestUpdate)},
        {0x5, SFUNC(IAudioRenderer::Start)},
        {0x6, SFUNC(IAudioRenderer::Stop)},
        {0x7, SFUNC(IAudioRenderer::QuerySystemEvent)},
    }) {
        track = state.audio->OpenTrack(constant::ChannelCount, params.sampleRate, [this]() { this->releaseEvent->Signal(); });
        track->Start();

        memoryPools.resize(memoryPoolCount);
        effects.resize(rendererParams.effectCount);
        voices.resize(rendererParams.voiceCount, Voice(state));

        // Fill track with empty samples that we will triple buffer
        track->AppendBuffer(std::vector<i16>(), 0);
        track->AppendBuffer(std::vector<i16>(), 1);
        track->AppendBuffer(std::vector<i16>(), 2);
    }

    IAudioRenderer::~IAudioRenderer() {
        state.audio->CloseTrack(track);
    }

    void IAudioRenderer::GetSampleRate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(rendererParams.sampleRate);
    }

    void IAudioRenderer::GetSampleCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(rendererParams.sampleCount);
    }

    void IAudioRenderer::GetMixBufferCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(rendererParams.subMixCount);
    }

    void IAudioRenderer::GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(static_cast<u32>(playbackState));
    }

    void IAudioRenderer::RequestUpdate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u64 inputAddress = request.inputBuf.at(0).address;

        auto inputHeader = state.process->GetObject<UpdateDataHeader>(inputAddress);
        revisionInfo.SetUserRevision(inputHeader.revision);
        inputAddress += sizeof(UpdateDataHeader);
        inputAddress += inputHeader.behaviorSize; // Unused

        std::vector<MemoryPoolIn> memoryPoolsIn(memoryPoolCount);
        state.process->ReadMemory(memoryPoolsIn.data(), inputAddress, memoryPoolCount * sizeof(MemoryPoolIn));
        inputAddress += inputHeader.memoryPoolSize;

        for (int i = 0; i < memoryPoolsIn.size(); i++)
            memoryPools[i].ProcessInput(memoryPoolsIn[i]);

        inputAddress += inputHeader.voiceResourceSize;
        std::vector<VoiceIn> voicesIn(rendererParams.voiceCount);
        state.process->ReadMemory(voicesIn.data(), inputAddress, rendererParams.voiceCount * sizeof(VoiceIn));
        inputAddress += inputHeader.voiceSize;

        for (int i = 0; i < voicesIn.size(); i++)
            voices[i].ProcessInput(voicesIn[i]);

        std::vector<EffectIn> effectsIn(rendererParams.effectCount);
        state.process->ReadMemory(effectsIn.data(), inputAddress, rendererParams.effectCount * sizeof(EffectIn));

        for (int i = 0; i < effectsIn.size(); i++)
            effects[i].ProcessInput(effectsIn[i]);

        UpdateAudio();

        UpdateDataHeader outputHeader{
            .revision = constant::RevMagic,
            .behaviorSize = 0xb0,
            .memoryPoolSize = (rendererParams.effectCount + rendererParams.voiceCount * 4) * static_cast<u32>(sizeof(MemoryPoolOut)),
            .voiceSize = rendererParams.voiceCount * static_cast<u32>(sizeof(VoiceOut)),
            .effectSize = rendererParams.effectCount * static_cast<u32>(sizeof(EffectOut)),
            .sinkSize = rendererParams.sinkCount * 0x20,
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

        for (auto &memoryPool : memoryPools) {
            state.process->WriteMemory(memoryPool.output, outputAddress);
            outputAddress += sizeof(MemoryPoolOut);
        }

        for (auto &voice : voices) {
            state.process->WriteMemory(voice.output, outputAddress);
            outputAddress += sizeof(VoiceOut);
        }

        for (auto &effect : effects) {
            state.process->WriteMemory(effect.output, outputAddress);
            outputAddress += sizeof(EffectOut);
        }
    }

    void IAudioRenderer::UpdateAudio() {
        std::vector<u64> released = track->GetReleasedBuffers(2);

        for (auto &tag : released) {
            MixFinalBuffer();
            track->AppendBuffer(sampleBuffer, tag);
        }
    }

    void IAudioRenderer::MixFinalBuffer() {
        int setIndex = 0;
        sampleBuffer.resize(static_cast<size_t>(samplesPerBuffer * constant::ChannelCount));

        for (auto &voice : voices) {
            if (!voice.Playable())
                continue;

            int bufferOffset = 0;
            int pendingSamples = samplesPerBuffer;

            while (pendingSamples > 0) {
                int voiceBufferSize = 0;
                int voiceBufferOffset = 0;
                std::vector<i16> &voiceSamples = voice.GetBufferData(pendingSamples, voiceBufferOffset, voiceBufferSize);

                if (voiceBufferSize == 0)
                    break;

                pendingSamples -= voiceBufferSize / constant::ChannelCount;

                for (int i = voiceBufferOffset; i < voiceBufferOffset + voiceBufferSize; i++) {
                    if (setIndex == bufferOffset) {
                        sampleBuffer[bufferOffset] = static_cast<i16>(std::clamp(static_cast<int>(static_cast<float>(voiceSamples[i]) *
                            voice.volume), static_cast<int>(std::numeric_limits<i16>::min()), static_cast<int>(std::numeric_limits<i16>::max())));

                        setIndex++;
                    } else {
                        sampleBuffer[bufferOffset] += static_cast<i16>(std::clamp(static_cast<int>(sampleBuffer[voiceSamples[i]]) +
                                                                                      static_cast<int>(static_cast<float>(voiceSamples[i]) * voice.volume),
                                                                                  static_cast<int>(std::numeric_limits<i16>::min()), static_cast<int>(std::numeric_limits<i16>::max())));
                    }

                    bufferOffset++;
                }
            }
        }
    }

    void IAudioRenderer::Start(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        playbackState = skyline::audio::AudioOutState::Started;
    }

    void IAudioRenderer::Stop(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        playbackState = skyline::audio::AudioOutState::Stopped;
    }

    void IAudioRenderer::QuerySystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle = state.process->InsertItem(releaseEvent);
        state.logger->Debug("Audren Buffer Release Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
    }
}
