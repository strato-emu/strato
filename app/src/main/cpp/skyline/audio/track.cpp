// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "track.h"

namespace skyline::audio {
    AudioTrack::AudioTrack(u8 channelCount, u32 sampleRate, const std::function<void()> &releaseCallback) : channelCount(channelCount), sampleRate(sampleRate), releaseCallback(releaseCallback) {
        if (sampleRate != constant::SampleRate)
            throw exception("Unsupported audio sample rate: {}", sampleRate);

        if (channelCount != constant::ChannelCount)
            throw exception("Unsupported quantity of audio channels: {}", channelCount);
    }

    void AudioTrack::Stop() {
        while (!identifiers.end()->released);
        playbackState = AudioOutState::Stopped;
    }

    bool AudioTrack::ContainsBuffer(u64 tag) {
        // Iterate from front of queue as we don't want released samples
        for (auto identifier = identifiers.crbegin(); identifier != identifiers.crend(); ++identifier) {
            if (identifier->released)
                return false;

            if (identifier->tag == tag)
                return true;
        }

        return false;
    }

    std::vector<u64> AudioTrack::GetReleasedBuffers(u32 max) {
        std::vector<u64> bufferIds;
        std::lock_guard trackGuard(bufferLock);

        for (u32 index{}; index < max; index++) {
            if (identifiers.empty() || !identifiers.back().released)
                break;
            bufferIds.push_back(identifiers.back().tag);
            identifiers.pop_back();
        }

        return bufferIds;
    }

    void AudioTrack::AppendBuffer(u64 tag, span<i16> buffer) {
        BufferIdentifier identifier{
            .released = false,
            .tag = tag,
            .finalSample = identifiers.empty() ? (buffer.size()) : (buffer.size() + identifiers.front().finalSample)
        };

        std::lock_guard guard(bufferLock);

        identifiers.push_front(identifier);
        samples.Append(buffer);
    }

    void AudioTrack::CheckReleasedBuffers() {
        bool anyReleased{};

        for (auto &identifier : identifiers) {
            if (identifier.finalSample <= sampleCounter && !identifier.released) {
                anyReleased = true;
                identifier.released = true;
            }
        }

        if (anyReleased)
            releaseCallback();
    }
}
