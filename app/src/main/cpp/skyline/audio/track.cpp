#include "track.h"

namespace skyline::audio {
    AudioTrack::AudioTrack(int channelCount, int sampleRate, const std::function<void()> &releaseCallback) : channelCount(channelCount), sampleRate(sampleRate), releaseCallback(releaseCallback) {
        if (sampleRate != constant::SampleRate)
            throw exception("Unsupported audio sample rate: {}", sampleRate);

        if (channelCount != constant::ChannelCount)
            throw exception("Unsupported quantity of audio channels: {}", channelCount);
    }

    void AudioTrack::Stop() {
        while (!identifierQueue.end()->released);
        playbackState = AudioOutState::Stopped;
    }

    bool AudioTrack::ContainsBuffer(u64 tag) {
        // Iterate from front of queue as we don't want released samples
        for (auto identifier = identifierQueue.crbegin(); identifier != identifierQueue.crend(); ++identifier) {
            if (identifier->released)
                return false;

            if (identifier->tag == tag)
                return true;
        }

        return false;
    }

    std::vector<u64> AudioTrack::GetReleasedBuffers(u32 max) {
        std::vector<u64> bufferIds;

        for (u32 i = 0; i < max; i++) {
            if (!identifierQueue.back().released)
                break;
            bufferIds.push_back(identifierQueue.back().tag);
            identifierQueue.pop_back();
        }

        return bufferIds;
    }

    void AudioTrack::AppendBuffer(const std::vector<i16> &sampleData, u64 tag) {
        BufferIdentifier identifier;

        identifier.released = false;
        identifier.tag = tag;

        if (identifierQueue.empty())
            identifier.finalSample = sampleData.size();
        else
            identifier.finalSample = sampleData.size() + identifierQueue.front().finalSample;

        bufferLock.lock();

        identifierQueue.push_front(identifier);
        for (auto &sample : sampleData)
            sampleQueue.push(sample);

        bufferLock.unlock();
    }

    void AudioTrack::CheckReleasedBuffers() {
        for (auto &identifier : identifierQueue) {
            if (identifier.finalSample <= sampleCounter && !identifier.released) {
                releaseCallback();
                identifier.released = true;
            }
        }
    }
}
