#include "audout.h"
#include <kernel/types/KProcess.h>

namespace skyline::service::audout {
    audoutU::audoutU(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::audout_u, "audout:u", {
        {0x0, SFUNC(audoutU::ListAudioOuts)},
        {0x1, SFUNC(audoutU::OpenAudioOut)}
    }) {}

    void audoutU::ListAudioOuts(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.process->WriteMemory(reinterpret_cast<void *>(const_cast<char *>(constant::DefaultAudioOutName.data())),
                                   request.outputBuf.at(0).address, constant::DefaultAudioOutName.size());
    }

    void audoutU::OpenAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 sampleRate = request.Pop<u32>();
        request.Pop<u16>(); // Channel count is stored in the upper half of a u32
        u16 channelCount = request.Pop<u16>();

        state.logger->Debug("audoutU: Opening an IAudioOut with sample rate: {}, channel count: {}", sampleRate, channelCount);

        sampleRate = sampleRate ? sampleRate : audio::constant::SampleRate;
        channelCount = channelCount ? channelCount : static_cast<u16>(audio::constant::ChannelCount);
        manager.RegisterService(std::make_shared<IAudioOut>(state, manager, channelCount, sampleRate), session, response);

        response.Push<u32>(sampleRate);
        response.Push<u16>(channelCount);
        response.Push<u16>(0);
        response.Push(static_cast<u32>(audio::PcmFormat::Int16));
        response.Push(static_cast<u32>(audio::AudioOutState::Stopped));
    }

    IAudioOut::IAudioOut(const DeviceState &state, ServiceManager &manager, int channelCount, int sampleRate) : sampleRate(sampleRate), channelCount(channelCount), releaseEvent(std::make_shared<type::KEvent>(state)), BaseService(state, manager, Service::audout_IAudioOut, "audout:IAudioOut", {
        {0x0, SFUNC(IAudioOut::GetAudioOutState)},
        {0x1, SFUNC(IAudioOut::StartAudioOut)},
        {0x2, SFUNC(IAudioOut::StopAudioOut)},
        {0x3, SFUNC(IAudioOut::AppendAudioOutBuffer)},
        {0x4, SFUNC(IAudioOut::RegisterBufferEvent)},
        {0x5, SFUNC(IAudioOut::GetReleasedAudioOutBuffer)},
        {0x6, SFUNC(IAudioOut::ContainsAudioOutBuffer)}
    }) {
        track = state.audio->OpenTrack(channelCount, audio::constant::SampleRate, [this]() { this->releaseEvent->Signal(); });
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
        u64 tag = request.Pop<u64>();

        state.logger->Debug("IAudioOut: Appending buffer with address: 0x{:X}, size: 0x{:X}", data.sampleBufferPtr, data.sampleSize);

        tmpSampleBuffer.resize(data.sampleSize / sizeof(i16));
        state.process->ReadMemory(tmpSampleBuffer.data(), data.sampleBufferPtr, data.sampleSize);
        resampler.ResampleBuffer(tmpSampleBuffer, static_cast<double>(sampleRate) / audio::constant::SampleRate, channelCount);
        track->AppendBuffer(tmpSampleBuffer, tag);
    }

    void IAudioOut::RegisterBufferEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle = state.process->InsertItem(releaseEvent);
        state.logger->Debug("Audout Buffer Release Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
    }

    void IAudioOut::GetReleasedAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 maxCount = static_cast<u32>(request.outputBuf.at(0).size >> 3);
        std::vector<u64> releasedBuffers = track->GetReleasedBuffers(maxCount);
        u32 count = static_cast<u32>(releasedBuffers.size());

        // Fill rest of output buffer with zeros
        releasedBuffers.resize(maxCount, 0);
        state.process->WriteMemory(releasedBuffers.data(), request.outputBuf.at(0).address, request.outputBuf.at(0).size);

        response.Push<u32>(count);
    }

    void IAudioOut::ContainsAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u64 tag = request.Pop<u64>();

        response.Push(static_cast<u32>(track->ContainsBuffer(tag)));
    }
}
