// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IAudioOutManager.h"
#include "IAudioOut.h"

namespace skyline::service::audio {
    IAudioOutManager::IAudioOutManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IAudioOutManager::ListAudioOuts(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.outputBuf.at(0).copy_from(constant::DefaultAudioOutName);
        return {};
    }

    Result IAudioOutManager::OpenAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto sampleRate{request.Pop<u32>()};
        auto channelCount{static_cast<u16>(request.Pop<u32>())};

        state.logger->Debug("Opening an IAudioOut with sample rate: {}, channel count: {}", sampleRate, channelCount);

        sampleRate = sampleRate ? sampleRate : constant::SampleRate;
        channelCount = channelCount ? channelCount : constant::ChannelCount;
        manager.RegisterService(std::make_shared<IAudioOut>(state, manager, channelCount, sampleRate), session, response);

        response.Push<u32>(sampleRate);
        response.Push<u16>(channelCount);
        response.Push<u16>(0);
        response.Push(static_cast<u32>(skyline::audio::AudioFormat::Int16));
        response.Push(static_cast<u32>(skyline::audio::AudioOutState::Stopped));

        return {};
    }
}
