// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "audio.h"

namespace skyline::audio {
    Audio::Audio(const DeviceState &state) : oboe::AudioStreamCallback() {
        builder.setChannelCount(constant::ChannelCount);
        builder.setSampleRate(constant::SampleRate);
        builder.setFormat(constant::PcmFormat);
        builder.setFramesPerCallback(constant::MixBufferSize);
        builder.setUsage(oboe::Usage::Game);
        builder.setCallback(this);

        builder.openManagedStream(outputStream);
        outputStream->requestStart();
    }

    Audio::~Audio() {
        outputStream->requestStop();
    }

    std::shared_ptr<AudioTrack> Audio::OpenTrack(u8 channelCount, u32 sampleRate, const std::function<void()> &releaseCallback) {
        std::lock_guard trackGuard(trackLock);

        auto track{std::make_shared<AudioTrack>(channelCount, sampleRate, releaseCallback)};
        audioTracks.push_back(track);

        return track;
    }

    void Audio::CloseTrack(std::shared_ptr<AudioTrack> &track) {
        std::lock_guard trackGuard(trackLock);

        audioTracks.erase(std::remove(audioTracks.begin(), audioTracks.end(), track), audioTracks.end());
        track.reset();
    }

    oboe::DataCallbackResult Audio::onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) {
        auto destBuffer{static_cast<i16 *>(audioData)};
        auto streamSamples{static_cast<size_t>(numFrames) * audioStream->getChannelCount()};
        size_t writtenSamples{};

        {
            std::lock_guard trackGuard(trackLock);

            for (auto &track : audioTracks) {
                if (track->playbackState == AudioOutState::Stopped)
                    continue;

                std::lock_guard bufferGuard(track->bufferLock);

                auto trackSamples{track->samples.Read(span(destBuffer, streamSamples), [](i16 *source, i16 *destination) {
                    *destination = Saturate<i16, i32>(static_cast<u32>(*destination) + static_cast<u32>(*source));
                }, writtenSamples)};

                writtenSamples = std::max(trackSamples, writtenSamples);

                track->sampleCounter += trackSamples;
                track->CheckReleasedBuffers();
            }
        }

        if (streamSamples > writtenSamples)
            memset(destBuffer + writtenSamples, 0, (streamSamples - writtenSamples) * sizeof(i16));

        return oboe::DataCallbackResult::Continue;
    }

    void Audio::onErrorAfterClose(oboe::AudioStream *audioStream, oboe::Result error) {
        if (error == oboe::Result::ErrorDisconnected) {
            builder.openManagedStream(outputStream);
            outputStream->requestStart();
        }
    }
}
