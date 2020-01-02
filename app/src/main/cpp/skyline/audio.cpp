#include "audio.h"

namespace skyline::audio {
    Audio::Audio(const DeviceState &state) : state(state), oboe::AudioStreamCallback() {
        oboe::AudioStreamBuilder builder;

        builder.setChannelCount(constant::ChannelCount)
            ->setSampleRate(constant::SampleRate)
            ->setFormat(constant::PcmFormat)
            ->setCallback(this)
            ->openManagedStream(outputStream);

        outputStream->requestStart();
    }

    std::shared_ptr<AudioTrack> Audio::OpenTrack(int channelCount, int sampleRate, const std::function<void()> &releaseCallback) {
        std::shared_ptr<AudioTrack> track = std::make_shared<AudioTrack>(channelCount, sampleRate, releaseCallback);
        audioTracks.push_back(track);

        return track;
    }

    void Audio::CloseTrack(std::shared_ptr<AudioTrack> &track) {
        audioTracks.erase(std::remove(audioTracks.begin(), audioTracks.end(), track), audioTracks.end());
        track.reset();
    }

    oboe::DataCallbackResult Audio::onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) {
        i16 *destBuffer = static_cast<i16 *>(audioData);
        int setIndex = 0;
        size_t sampleI16Size = static_cast<size_t>(numFrames) * audioStream->getChannelCount();

        for (auto &track : audioTracks) {
            if (track->playbackState == AudioOutState::Stopped)
                continue;

            track->bufferLock.lock();

            std::queue<i16> &srcBuffer = track->sampleQueue;
            size_t amount = std::min(srcBuffer.size(), sampleI16Size);

            for (size_t i = 0; i < amount; i++) {
                if (setIndex == i) {
                    destBuffer[i] = srcBuffer.front();
                    setIndex++;
                } else {
                    destBuffer[i] += srcBuffer.front();
                }

                srcBuffer.pop();
            }

            track->sampleCounter += amount;
            track->CheckReleasedBuffers();

            track->bufferLock.unlock();
        }

        if (sampleI16Size > setIndex)
            memset(destBuffer, 0, (sampleI16Size - setIndex) * 2);

        return oboe::DataCallbackResult::Continue;
    }
}
