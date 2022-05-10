// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "downmixer.h"
#include "track.h"

namespace skyline::audio {
    AudioTrack::AudioTrack(u8 channelCount, u32 sampleRate, std::function<void()> releaseCallback)
        : channelCount(channelCount),
          sampleRate(sampleRate),
          releaseCallback(std::move(releaseCallback)) {
        if (sampleRate != constant::SampleRate)
            throw exception("Unsupported audio sample rate: {}", sampleRate);

        if (channelCount != constant::StereoChannelCount && channelCount != constant::SurroundChannelCount)
            throw exception("Unsupported quantity of audio channels: {}", channelCount);
    }

    void AudioTrack::Stop() {
        auto allSamplesReleased{[&]() {
            std::scoped_lock lock{bufferLock};
            return identifiers.empty() || identifiers.end()->released;
        }};

        while (!allSamplesReleased());
        playbackState = AudioOutState::Stopped;
    }

    bool AudioTrack::ContainsBuffer(u64 tag) {
        std::scoped_lock lock(bufferLock);

        // Iterate from front of queue as we don't want released samples
        for (auto identifier{identifiers.crbegin()}; identifier != identifiers.crend(); identifier++) {
            if (identifier->released)
                return false;

            if (identifier->tag == tag)
                return true;
        }

        return false;
    }

    std::vector<u64> AudioTrack::GetReleasedBuffers(u32 max) {
        std::vector<u64> bufferIds;
        std::scoped_lock lock(bufferLock);

        for (u32 index{}; index < max; index++) {
            if (identifiers.empty() || !identifiers.back().released)
                break;
            bufferIds.push_back(identifiers.back().tag);
            identifiers.pop_back();
        }

        return bufferIds;
    }

    void AudioTrack::AppendBuffer(u64 tag, span<i16> buffer) {
        std::scoped_lock lock(bufferLock);

        size_t size{(channelCount == constant::SurroundChannelCount) ? (buffer.size() / sizeof(Surround51Sample)) * sizeof(StereoSample) : buffer.size()};
        BufferIdentifier identifier{
            .released = false,
            .tag = tag,
            .finalSample = identifiers.empty() ? size : (size + identifiers.front().finalSample)
        };

        identifiers.push_front(identifier);

        if (channelCount == constant::SurroundChannelCount) {
            auto stereoBuffer{DownMix(buffer.cast<Surround51Sample>())};
            samples.Append(span(stereoBuffer).cast<i16>());
        } else {
            samples.Append(buffer);
        }
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
