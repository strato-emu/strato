// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "audio.h"

namespace skyline::audio {
    Audio::Audio(const DeviceState &state) : oboe::AudioStreamCallback() {
        settings = std::shared_ptr<Settings>{state.settings};

        builder.setChannelCount(constant::StereoChannelCount);
        builder.setSampleRate(constant::SampleRate);
        builder.setFormat(constant::PcmFormat);
        builder.setUsage(oboe::Usage::Game);
        builder.setCallback(this);
        builder.setSharingMode(oboe::SharingMode::Exclusive);
        builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);

        builder.openManagedStream(outputStream);
        outputStream->requestStart();
    }

    Audio::~Audio() {
        outputStream->requestStop();
    }

    std::shared_ptr<AudioTrack> Audio::OpenTrack(u8 channelCount, u32 sampleRate, const std::function<void()> &releaseCallback) {
        std::scoped_lock trackGuard{trackLock};

        auto track{std::make_shared<AudioTrack>(channelCount, sampleRate, releaseCallback)};
        audioTracks.push_back(track);

        return track;
    }

    void Audio::CloseTrack(std::shared_ptr<AudioTrack> &track) {
        std::scoped_lock trackGuard{trackLock};

        audioTracks.erase(std::remove(audioTracks.begin(), audioTracks.end(), track), audioTracks.end());
    }

    oboe::DataCallbackResult Audio::onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) {
        auto destBuffer{static_cast<i16 *>(audioData)};
        auto streamSamples{static_cast<size_t>(numFrames) * static_cast<size_t>(audioStream->getChannelCount())};
        size_t writtenSamples{};

        {
            std::scoped_lock trackGuard{trackLock};

            for (auto &track : audioTracks) {
                if (track->playbackState == AudioOutState::Stopped)
                    continue;

                std::scoped_lock bufferGuard{track->bufferLock};

                if (!*settings->isAudioOutputDisabled) {
                    auto trackSamples{track->samples.Read(span(destBuffer, streamSamples), [](i16 *source, i16 *destination) {
                        *destination = Saturate<i16, i32>(static_cast<u32>(*destination) + static_cast<u32>(*source));
                    }, static_cast<ssize_t>(writtenSamples))};

                    writtenSamples = std::max(trackSamples, writtenSamples);

                    track->sampleCounter += trackSamples;
                } else {
                    track->sampleCounter += streamSamples;
                }
                track->CheckReleasedBuffers();
            }
        }

        if (streamSamples > writtenSamples)
            memset(destBuffer + writtenSamples, 0, (streamSamples - writtenSamples) * sizeof(i16));

        return oboe::DataCallbackResult::Continue;
    }

    void Audio::onErrorAfterClose(oboe::AudioStream *audioStream, oboe::Result error) {
        builder.openManagedStream(outputStream);
        outputStream->requestStart();
    }
}
