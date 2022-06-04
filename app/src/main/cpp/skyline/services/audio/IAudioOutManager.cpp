// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IAudioOutManager.h"
#include "IAudioOut.h"

namespace skyline::service::audio {
    IAudioOutManager::IAudioOutManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IAudioOutManager::ListAudioOuts(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::memset(request.outputBuf.at(0).data(), 0, request.outputBuf.at(0).size());
        request.outputBuf.at(0).copy_from(constant::DefaultAudioOutName);
        response.Push<u32>(1); // One audio out
        return {};
    }

    Result IAudioOutManager::OpenAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct AudioInputParams {
            u32 sampleRate;
            u16 channelCount;
            u16 _pad_;
        };
        auto &inputParams{request.Pop<AudioInputParams>()};

        inputParams.sampleRate = inputParams.sampleRate ? inputParams.sampleRate : constant::SampleRate;
        inputParams.channelCount = inputParams.channelCount <= constant::StereoChannelCount ? constant::StereoChannelCount : constant::SurroundChannelCount;
        inputParams._pad_ = 0;
        Logger::Debug("Opening an IAudioOut with sample rate: {}, channel count: {}", inputParams.sampleRate, inputParams.channelCount);
        manager.RegisterService(std::make_shared<IAudioOut>(state, manager, inputParams.channelCount, inputParams.sampleRate), session, response);
        response.Push<AudioInputParams>(inputParams);
        response.Push(static_cast<u32>(skyline::audio::AudioFormat::Int16));
        response.Push(static_cast<u32>(skyline::audio::AudioOutState::Stopped));

        std::memset(request.outputBuf.at(0).data(), 0, request.outputBuf.at(0).size());

        if (request.inputBuf.at(0).empty() || !request.inputBuf.at(0)[0])
            request.outputBuf.at(0).copy_from(constant::DefaultAudioOutName);
        else
            request.outputBuf.at(0).copy_from(request.inputBuf.at(0));

        return {};
    }
}
